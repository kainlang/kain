use std::collections::HashMap;
use std::sync::{Arc, Once, RwLock};

use kain_core::error::{KainError, KainResult};
use kain_core::runtime::{register_env_extension, Env, Value};
use kain_core::stdlib::{register_stdlib_extension, BuiltinFn, StdLib};
use pyo3::prelude::*;
use pyo3::types::{PyByteArray, PyBytes, PyDict, PyList, PyTuple};

const PYTHON_EXTENSION_KEY: &str = "kain.python.scope";

static REGISTER: Once = Once::new();

const IMAGE_CHANNEL_COUNTS: &[i64] = &[1, 2, 3, 4];

struct PythonScopeState {
    scope: RwLock<PyObject>,
}

#[derive(Clone)]
struct PythonObjectRef {
    object: PyObject,
}

#[derive(Clone)]
struct PythonImageView {
    object: PyObject,
    layout: String,
    batch: i64,
    width: i64,
    height: i64,
    channels: i64,
}

#[derive(Clone)]
struct PythonTensorView {
    object: PyObject,
    metadata: PythonPayloadMetadata,
}

#[derive(Clone)]
struct PythonGeometryView {
    vertices: PyObject,
    indices: Option<PyObject>,
    vertex_metadata: PythonPayloadMetadata,
    index_metadata: Option<PythonPayloadMetadata>,
    components: i64,
    face_size: i64,
}

#[derive(Clone, Copy)]
enum SharedScalarKind {
    Bool,
    Int,
    Float,
}

#[derive(Clone)]
struct SharedPythonBuffer {
    owner: PyObject,
    flat: PyObject,
    backend: String,
    kind: SharedScalarKind,
    len: usize,
}

#[derive(Clone)]
enum NativeScalarBuffer {
    Bool(Arc<RwLock<Vec<bool>>>),
    Int(Arc<RwLock<Vec<i64>>>),
    Float(Arc<RwLock<Vec<f64>>>),
    Shared(Arc<SharedPythonBuffer>),
}

#[derive(Clone)]
struct KainNativeImage {
    dtype: String,
    shape: Vec<i64>,
    layout: String,
    batch: i64,
    width: i64,
    height: i64,
    channels: i64,
    data: NativeScalarBuffer,
    source: Option<PyObject>,
}

#[derive(Clone)]
struct KainNativeTensor {
    dtype: String,
    shape: Vec<i64>,
    data: NativeScalarBuffer,
    source: Option<PyObject>,
}

#[derive(Clone)]
struct KainNativeGeometry {
    vertex_dtype: String,
    vertex_shape: Vec<i64>,
    components: i64,
    vertices: NativeScalarBuffer,
    index_dtype: Option<String>,
    index_shape: Vec<i64>,
    face_size: i64,
    indices: Option<NativeScalarBuffer>,
    source: Option<PyObject>,
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
        BuiltinFn {
            name: "py_tensor_info",
            params: vec![("target", "Any")],
            return_type: "Any",
            doc: "Inspect a tensor or typed numeric payload across NumPy and PyTorch",
        },
        BuiltinFn {
            name: "py_tensor_bytes",
            params: vec![("target", "Any")],
            return_type: "Any",
            doc: "Snapshot a tensor payload as flat bytes across NumPy and PyTorch",
        },
        BuiltinFn {
            name: "py_image_info",
            params: vec![("target", "Any")],
            return_type: "Any",
            doc: "Infer width, height, channels, layout, and storage info for an image payload",
        },
        BuiltinFn {
            name: "py_image_view",
            params: vec![("target", "Any")],
            return_type: "Any",
            doc: "Create a read-only typed image view over a Python image payload",
        },
        BuiltinFn {
            name: "py_image_pixel",
            params: vec![("view", "Any"), ("x", "Int"), ("y", "Int")],
            return_type: "Any",
            doc: "Read a pixel from a typed Python image view without flattening the whole payload",
        },
        BuiltinFn {
            name: "py_image_set_pixel",
            params: vec![
                ("view", "Any"),
                ("x", "Int"),
                ("y", "Int"),
                ("value", "Any"),
            ],
            return_type: "Unit",
            doc: "Write a pixel into a live Python image view",
        },
        BuiltinFn {
            name: "py_geometry_info",
            params: vec![("target", "Any")],
            return_type: "Any",
            doc: "Infer vertex/index layout for array-backed geometry or mesh objects",
        },
        BuiltinFn {
            name: "py_geometry_view",
            params: vec![("target", "Any")],
            return_type: "Any",
            doc: "Create a read-only typed geometry view over Python mesh or point-cloud data",
        },
        BuiltinFn {
            name: "py_geometry_vertex",
            params: vec![("view", "Any"), ("index", "Int")],
            return_type: "Any",
            doc: "Read one vertex from a typed Python geometry view",
        },
        BuiltinFn {
            name: "py_geometry_face",
            params: vec![("view", "Any"), ("index", "Int")],
            return_type: "Any",
            doc: "Read one face from a typed Python geometry view",
        },
        BuiltinFn {
            name: "py_geometry_set_vertex",
            params: vec![("view", "Any"), ("index", "Int"), ("value", "Any")],
            return_type: "Unit",
            doc: "Write one vertex into a live Python geometry view",
        },
        BuiltinFn {
            name: "py_geometry_set_face",
            params: vec![("view", "Any"), ("index", "Int"), ("value", "Any")],
            return_type: "Unit",
            doc: "Write one face into a live Python geometry view",
        },
        BuiltinFn {
            name: "py_tensor_view",
            params: vec![("target", "Any")],
            return_type: "Any",
            doc: "Create a read-only typed tensor view over a Python tensor payload",
        },
        BuiltinFn {
            name: "py_tensor_get",
            params: vec![("view", "Any"), ("indices", "Any")],
            return_type: "Any",
            doc: "Read one tensor element or sub-value from a typed Python tensor view",
        },
        BuiltinFn {
            name: "py_tensor_set",
            params: vec![("view", "Any"), ("indices", "Any"), ("value", "Any")],
            return_type: "Unit",
            doc: "Write one tensor element or sub-value into a live Python tensor view",
        },
        BuiltinFn {
            name: "kain_image_from_py",
            params: vec![("target", "Any")],
            return_type: "Any",
            doc: "Materialize a Python image payload into a Kain-owned typed image buffer",
        },
        BuiltinFn {
            name: "kain_image_info",
            params: vec![("image", "Any")],
            return_type: "Any",
            doc: "Inspect a Kain-owned typed image buffer",
        },
        BuiltinFn {
            name: "kain_image_pixel",
            params: vec![("image", "Any"), ("x", "Int"), ("y", "Int")],
            return_type: "Any",
            doc: "Read one pixel from a Kain-owned typed image buffer",
        },
        BuiltinFn {
            name: "kain_image_set_pixel",
            params: vec![
                ("image", "Any"),
                ("x", "Int"),
                ("y", "Int"),
                ("value", "Any"),
            ],
            return_type: "Unit",
            doc: "Write one pixel into a Kain-owned typed image buffer",
        },
        BuiltinFn {
            name: "kain_image_to_py",
            params: vec![("image", "Any"), ("backend", "String")],
            return_type: "Any",
            doc: "Export a Kain-owned typed image buffer back into a Python object such as a NumPy array or torch tensor",
        },
        BuiltinFn {
            name: "kain_tensor_from_py",
            params: vec![("target", "Any")],
            return_type: "Any",
            doc: "Materialize a Python tensor payload into a Kain-owned typed tensor buffer",
        },
        BuiltinFn {
            name: "kain_tensor_info",
            params: vec![("tensor", "Any")],
            return_type: "Any",
            doc: "Inspect a Kain-owned typed tensor buffer",
        },
        BuiltinFn {
            name: "kain_tensor_get",
            params: vec![("tensor", "Any"), ("indices", "Any")],
            return_type: "Any",
            doc: "Read one tensor element from a Kain-owned typed tensor buffer",
        },
        BuiltinFn {
            name: "kain_tensor_set",
            params: vec![("tensor", "Any"), ("indices", "Any"), ("value", "Any")],
            return_type: "Unit",
            doc: "Write one tensor element into a Kain-owned typed tensor buffer",
        },
        BuiltinFn {
            name: "kain_tensor_to_py",
            params: vec![("tensor", "Any"), ("backend", "String")],
            return_type: "Any",
            doc: "Export a Kain-owned typed tensor buffer back into a Python object such as a NumPy array or torch tensor",
        },
        BuiltinFn {
            name: "kain_geometry_from_py",
            params: vec![("target", "Any")],
            return_type: "Any",
            doc: "Materialize Python geometry into a Kain-owned typed geometry buffer",
        },
        BuiltinFn {
            name: "kain_geometry_info",
            params: vec![("geometry", "Any")],
            return_type: "Any",
            doc: "Inspect a Kain-owned typed geometry buffer",
        },
        BuiltinFn {
            name: "kain_geometry_vertex",
            params: vec![("geometry", "Any"), ("index", "Int")],
            return_type: "Any",
            doc: "Read one vertex from a Kain-owned typed geometry buffer",
        },
        BuiltinFn {
            name: "kain_geometry_set_vertex",
            params: vec![("geometry", "Any"), ("index", "Int"), ("value", "Any")],
            return_type: "Unit",
            doc: "Write one vertex into a Kain-owned typed geometry buffer",
        },
        BuiltinFn {
            name: "kain_geometry_face",
            params: vec![("geometry", "Any"), ("index", "Int")],
            return_type: "Any",
            doc: "Read one face from a Kain-owned typed geometry buffer",
        },
        BuiltinFn {
            name: "kain_geometry_set_face",
            params: vec![("geometry", "Any"), ("index", "Int"), ("value", "Any")],
            return_type: "Unit",
            doc: "Write one face into a Kain-owned typed geometry buffer",
        },
        BuiltinFn {
            name: "kain_geometry_to_py",
            params: vec![("geometry", "Any"), ("backend", "String")],
            return_type: "Any",
            doc: "Export a Kain-owned typed geometry buffer back into Python arrays, dict payloads, or trimesh objects",
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
    env.register_native_fn("py_tensor_info", py_tensor_info_native);
    env.register_native_fn("py_tensor_bytes", py_tensor_bytes_native);
    env.register_native_fn("py_image_info", py_image_info_native);
    env.register_native_fn("py_image_view", py_image_view_native);
    env.register_native_fn("py_image_pixel", py_image_pixel_native);
    env.register_native_fn("py_image_set_pixel", py_image_set_pixel_native);
    env.register_native_fn("py_geometry_info", py_geometry_info_native);
    env.register_native_fn("py_geometry_view", py_geometry_view_native);
    env.register_native_fn("py_geometry_vertex", py_geometry_vertex_native);
    env.register_native_fn("py_geometry_face", py_geometry_face_native);
    env.register_native_fn("py_geometry_set_vertex", py_geometry_set_vertex_native);
    env.register_native_fn("py_geometry_set_face", py_geometry_set_face_native);
    env.register_native_fn("py_tensor_view", py_tensor_view_native);
    env.register_native_fn("py_tensor_get", py_tensor_get_native);
    env.register_native_fn("py_tensor_set", py_tensor_set_native);
    env.register_native_fn("kain_image_from_py", kain_image_from_py_native);
    env.register_native_fn("kain_image_info", kain_image_info_native);
    env.register_native_fn("kain_image_pixel", kain_image_pixel_native);
    env.register_native_fn("kain_image_set_pixel", kain_image_set_pixel_native);
    env.register_native_fn("kain_image_to_py", kain_image_to_py_native);
    env.register_native_fn("kain_tensor_from_py", kain_tensor_from_py_native);
    env.register_native_fn("kain_tensor_info", kain_tensor_info_native);
    env.register_native_fn("kain_tensor_get", kain_tensor_get_native);
    env.register_native_fn("kain_tensor_set", kain_tensor_set_native);
    env.register_native_fn("kain_tensor_to_py", kain_tensor_to_py_native);
    env.register_native_fn("kain_geometry_from_py", kain_geometry_from_py_native);
    env.register_native_fn("kain_geometry_info", kain_geometry_info_native);
    env.register_native_fn("kain_geometry_vertex", kain_geometry_vertex_native);
    env.register_native_fn("kain_geometry_set_vertex", kain_geometry_set_vertex_native);
    env.register_native_fn("kain_geometry_face", kain_geometry_face_native);
    env.register_native_fn("kain_geometry_set_face", kain_geometry_set_face_native);
    env.register_native_fn("kain_geometry_to_py", kain_geometry_to_py_native);
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
        if is_torch_tensor(target.as_ref(py)) {
            let metadata = resolve_payload_metadata(py, target.as_ref(py))?;
            Ok(metadata_to_value("PyBufferInfo", &metadata))
        } else {
            let view = create_memoryview(py, target.as_ref(py))?;
            build_buffer_info(target.as_ref(py), view.as_ref(py))
        }
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
        let bytes = export_payload_bytes(py, target.as_ref(py))?;
        py_to_value(bytes.as_ref(py))
    })
}

fn py_tensor_info_native(env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    if args.len() != 1 {
        return Err(KainError::runtime(
            "py_tensor_info: expected 1 argument (target)",
        ));
    }

    let state = python_scope_state(env)?;
    Python::with_gil(|py| {
        let scope = state.scope.read().unwrap();
        let scope_dict = scope_dict_from_guard(py, &scope)?;
        let target = resolve_python_target(py, scope_dict, &args[0])?;
        let metadata = resolve_payload_metadata(py, target.as_ref(py))?;
        Ok(metadata_to_value("PyTensorInfo", &metadata))
    })
}

fn py_tensor_bytes_native(env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    if args.len() != 1 {
        return Err(KainError::runtime(
            "py_tensor_bytes: expected 1 argument (target)",
        ));
    }

    let state = python_scope_state(env)?;
    Python::with_gil(|py| {
        let scope = state.scope.read().unwrap();
        let scope_dict = scope_dict_from_guard(py, &scope)?;
        let target = resolve_python_target(py, scope_dict, &args[0])?;
        let bytes = export_payload_bytes(py, target.as_ref(py))?;
        py_to_value(bytes.as_ref(py))
    })
}

fn py_image_info_native(env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    if args.len() != 1 {
        return Err(KainError::runtime(
            "py_image_info: expected 1 argument (target)",
        ));
    }

    let state = python_scope_state(env)?;
    Python::with_gil(|py| {
        let scope = state.scope.read().unwrap();
        let scope_dict = scope_dict_from_guard(py, &scope)?;
        let target = resolve_python_target(py, scope_dict, &args[0])?;
        let metadata = resolve_payload_metadata(py, target.as_ref(py))?;
        build_image_info(&metadata)
    })
}

fn py_geometry_info_native(env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    if args.is_empty() || args.len() > 2 {
        return Err(KainError::runtime(
            "py_geometry_info: expected (target) or (vertices, indices)",
        ));
    }

    let state = python_scope_state(env)?;
    Python::with_gil(|py| {
        let scope = state.scope.read().unwrap();
        let scope_dict = scope_dict_from_guard(py, &scope)?;
        let (vertices, indices) = resolve_geometry_targets(py, scope_dict, &args)?;
        let vertex_metadata = resolve_payload_metadata(py, vertices.as_ref(py))?;
        let index_metadata = indices
            .as_ref()
            .map(|value| resolve_payload_metadata(py, value.as_ref(py)))
            .transpose()?;
        build_geometry_info(&vertex_metadata, index_metadata.as_ref())
    })
}

fn py_image_view_native(env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    if args.len() != 1 {
        return Err(KainError::runtime(
            "py_image_view: expected 1 argument (target)",
        ));
    }

    let state = python_scope_state(env)?;
    Python::with_gil(|py| {
        let scope = state.scope.read().unwrap();
        let scope_dict = scope_dict_from_guard(py, &scope)?;
        let target = resolve_python_target(py, scope_dict, &args[0])?;
        let metadata = resolve_payload_metadata(py, target.as_ref(py))?;
        build_image_view(target, &metadata)
    })
}

fn py_image_pixel_native(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let (view, batch, x, y) = match args.as_slice() {
        [view, x, y] => (
            extract_image_view(view)?,
            0,
            value_to_int_arg("py_image_pixel", "x", x)?,
            value_to_int_arg("py_image_pixel", "y", y)?,
        ),
        [view, batch, x, y] => (
            extract_image_view(view)?,
            value_to_int_arg("py_image_pixel", "batch", batch)?,
            value_to_int_arg("py_image_pixel", "x", x)?,
            value_to_int_arg("py_image_pixel", "y", y)?,
        ),
        _ => {
            return Err(KainError::runtime(
                "py_image_pixel: expected (view, x, y) or (view, batch, x, y)",
            ))
        }
    };

    Python::with_gil(|py| {
        let index = image_pixel_indices(py, &view, batch, x, y)?;
        let pixel = get_python_item(py, view.object.as_ref(py), &index)?;
        let pixel = normalize_vector_value(py_any_to_value(pixel.as_ref(py))?);
        validate_vector_length("py_image_pixel", &pixel, view.channels)?;
        Ok(pixel)
    })
}

fn py_image_set_pixel_native(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let (view, batch, x, y, value) =
        match args.as_slice() {
            [view, x, y, value] => (
                extract_image_view(view)?,
                0,
                value_to_int_arg("py_image_set_pixel", "x", x)?,
                value_to_int_arg("py_image_set_pixel", "y", y)?,
                value,
            ),
            [view, batch, x, y, value] => (
                extract_image_view(view)?,
                value_to_int_arg("py_image_set_pixel", "batch", batch)?,
                value_to_int_arg("py_image_set_pixel", "x", x)?,
                value_to_int_arg("py_image_set_pixel", "y", y)?,
                value,
            ),
            _ => return Err(KainError::runtime(
                "py_image_set_pixel: expected (view, x, y, value) or (view, batch, x, y, value)",
            )),
        };

    validate_pixel_value("py_image_set_pixel", value, view.channels)?;
    Python::with_gil(|py| {
        let index = image_pixel_indices(py, &view, batch, x, y)?;
        let py_value = python_pixel_value(py, value, view.channels)?;
        set_python_item(py, view.object.as_ref(py), &index, py_value)?;
        Ok(Value::Unit)
    })
}

fn py_tensor_view_native(env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    if args.len() != 1 {
        return Err(KainError::runtime(
            "py_tensor_view: expected 1 argument (target)",
        ));
    }

    let state = python_scope_state(env)?;
    Python::with_gil(|py| {
        let scope = state.scope.read().unwrap();
        let scope_dict = scope_dict_from_guard(py, &scope)?;
        let target = resolve_python_target(py, scope_dict, &args[0])?;
        let metadata = resolve_payload_metadata(py, target.as_ref(py))?;
        Ok(Value::host_object(
            "python:view:tensor",
            Arc::new(PythonTensorView {
                object: target,
                metadata,
            }),
        ))
    })
}

fn py_tensor_get_native(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    if args.len() != 2 {
        return Err(KainError::runtime(
            "py_tensor_get: expected (view, indices)",
        ));
    }

    let view = extract_tensor_view(&args[0])?;
    let indices = parse_index_values("py_tensor_get", &args[1])?;
    Python::with_gil(|py| {
        validate_rank(
            "py_tensor_get",
            &view.metadata.shape,
            indices.len(),
            "tensor rank",
        )?;
        let py_indices = indices
            .iter()
            .map(|value| (*value).into_py(py))
            .collect::<Vec<_>>();
        let item = get_python_item(py, view.object.as_ref(py), &py_indices)?;
        py_any_to_value(item.as_ref(py))
    })
}

fn py_tensor_set_native(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    if args.len() != 3 {
        return Err(KainError::runtime(
            "py_tensor_set: expected (view, indices, value)",
        ));
    }

    let view = extract_tensor_view(&args[0])?;
    let indices = parse_index_values("py_tensor_set", &args[1])?;
    validate_rank(
        "py_tensor_set",
        &view.metadata.shape,
        indices.len(),
        "tensor rank",
    )?;
    Python::with_gil(|py| {
        let py_indices = indices
            .iter()
            .map(|value| (*value).into_py(py))
            .collect::<Vec<_>>();
        let py_value = value_to_pyobject(py, &args[2])?;
        set_python_item(py, view.object.as_ref(py), &py_indices, py_value)?;
        Ok(Value::Unit)
    })
}

fn py_geometry_view_native(env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    if args.is_empty() || args.len() > 2 {
        return Err(KainError::runtime(
            "py_geometry_view: expected (target) or (vertices, indices)",
        ));
    }

    let state = python_scope_state(env)?;
    Python::with_gil(|py| {
        let scope = state.scope.read().unwrap();
        let scope_dict = scope_dict_from_guard(py, &scope)?;
        let (vertices, indices) = resolve_geometry_targets(py, scope_dict, &args)?;
        let vertex_metadata = resolve_payload_metadata(py, vertices.as_ref(py))?;
        let index_metadata = indices
            .as_ref()
            .map(|value| resolve_payload_metadata(py, value.as_ref(py)))
            .transpose()?;
        build_geometry_view(vertices, indices, vertex_metadata, index_metadata)
    })
}

fn py_geometry_vertex_native(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    if args.len() != 2 {
        return Err(KainError::runtime(
            "py_geometry_vertex: expected (view, index)",
        ));
    }

    let view = extract_geometry_view(&args[0])?;
    let index = value_to_int_arg("py_geometry_vertex", "index", &args[1])?;
    validate_axis_index(
        "py_geometry_vertex",
        index,
        view.vertex_metadata.shape.first().copied().unwrap_or(0),
    )?;
    Python::with_gil(|py| {
        let item = get_python_item(py, view.vertices.as_ref(py), &[index.into_py(py)])?;
        let point = normalize_vector_value(py_any_to_value(item.as_ref(py))?);
        validate_vector_length("py_geometry_vertex", &point, view.components)?;
        Ok(point)
    })
}

fn py_geometry_face_native(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    if args.len() != 2 {
        return Err(KainError::runtime(
            "py_geometry_face: expected (view, index)",
        ));
    }

    let view = extract_geometry_view(&args[0])?;
    let Some(indices) = view.indices.as_ref() else {
        return Err(KainError::runtime(
            "py_geometry_face: geometry view has no index buffer",
        ));
    };
    let index = value_to_int_arg("py_geometry_face", "index", &args[1])?;
    let face_count = view
        .index_metadata
        .as_ref()
        .and_then(|metadata| metadata.shape.first().copied())
        .unwrap_or(0);
    validate_axis_index("py_geometry_face", index, face_count)?;
    Python::with_gil(|py| {
        let item = get_python_item(py, indices.as_ref(py), &[index.into_py(py)])?;
        let face = normalize_vector_value(py_any_to_value(item.as_ref(py))?);
        if view.face_size > 0 {
            validate_vector_length("py_geometry_face", &face, view.face_size)?;
        }
        Ok(face)
    })
}

fn py_geometry_set_vertex_native(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    if args.len() != 3 {
        return Err(KainError::runtime(
            "py_geometry_set_vertex: expected (view, index, value)",
        ));
    }

    let view = extract_geometry_view(&args[0])?;
    let index = value_to_int_arg("py_geometry_set_vertex", "index", &args[1])?;
    validate_axis_index(
        "py_geometry_set_vertex",
        index,
        view.vertex_metadata.shape.first().copied().unwrap_or(0),
    )?;
    validate_vector_value("py_geometry_set_vertex", &args[2], view.components)?;
    Python::with_gil(|py| {
        let py_value = value_to_pyobject(py, &args[2])?;
        set_python_item(py, view.vertices.as_ref(py), &[index.into_py(py)], py_value)?;
        Ok(Value::Unit)
    })
}

fn py_geometry_set_face_native(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    if args.len() != 3 {
        return Err(KainError::runtime(
            "py_geometry_set_face: expected (view, index, value)",
        ));
    }

    let view = extract_geometry_view(&args[0])?;
    let Some(indices) = view.indices.as_ref() else {
        return Err(KainError::runtime(
            "py_geometry_set_face: geometry view has no index buffer",
        ));
    };
    let index = value_to_int_arg("py_geometry_set_face", "index", &args[1])?;
    let face_count = view
        .index_metadata
        .as_ref()
        .and_then(|metadata| metadata.shape.first().copied())
        .unwrap_or(0);
    validate_axis_index("py_geometry_set_face", index, face_count)?;
    if view.face_size > 0 {
        validate_vector_value("py_geometry_set_face", &args[2], view.face_size)?;
    }
    Python::with_gil(|py| {
        let py_value = value_to_pyobject(py, &args[2])?;
        set_python_item(py, indices.as_ref(py), &[index.into_py(py)], py_value)?;
        Ok(Value::Unit)
    })
}

fn kain_image_from_py_native(env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    if args.len() != 1 {
        return Err(KainError::runtime(
            "kain_image_from_py: expected 1 argument (target)",
        ));
    }

    let state = python_scope_state(env)?;
    Python::with_gil(|py| {
        let scope = state.scope.read().unwrap();
        let scope_dict = scope_dict_from_guard(py, &scope)?;
        let target = resolve_python_target(py, scope_dict, &args[0])?;
        let metadata = resolve_payload_metadata(py, target.as_ref(py))?;
        build_native_image(target.as_ref(py), &metadata)
    })
}

fn kain_image_info_native(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    if args.len() != 1 {
        return Err(KainError::runtime(
            "kain_image_info: expected 1 argument (image)",
        ));
    }

    let image = extract_native_image(&args[0])?;
    Ok(native_image_info_value(&image))
}

fn kain_image_pixel_native(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let (image, batch, x, y) = match args.as_slice() {
        [image, x, y] => (
            extract_native_image(image)?,
            0,
            value_to_int_arg("kain_image_pixel", "x", x)?,
            value_to_int_arg("kain_image_pixel", "y", y)?,
        ),
        [image, batch, x, y] => (
            extract_native_image(image)?,
            value_to_int_arg("kain_image_pixel", "batch", batch)?,
            value_to_int_arg("kain_image_pixel", "x", x)?,
            value_to_int_arg("kain_image_pixel", "y", y)?,
        ),
        _ => {
            return Err(KainError::runtime(
                "kain_image_pixel: expected (image, x, y) or (image, batch, x, y)",
            ))
        }
    };

    native_image_pixel(&image, batch, x, y)
}

fn kain_image_set_pixel_native(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let (image, batch, x, y, value) = match args.as_slice() {
        [image, x, y, value] => (
            extract_native_image(image)?,
            0,
            value_to_int_arg("kain_image_set_pixel", "x", x)?,
            value_to_int_arg("kain_image_set_pixel", "y", y)?,
            value,
        ),
        [image, batch, x, y, value] => (
            extract_native_image(image)?,
            value_to_int_arg("kain_image_set_pixel", "batch", batch)?,
            value_to_int_arg("kain_image_set_pixel", "x", x)?,
            value_to_int_arg("kain_image_set_pixel", "y", y)?,
            value,
        ),
        _ => return Err(KainError::runtime(
            "kain_image_set_pixel: expected (image, x, y, value) or (image, batch, x, y, value)",
        )),
    };

    native_image_set_pixel(&image, batch, x, y, value)?;
    Ok(Value::Unit)
}

fn kain_tensor_from_py_native(env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    if args.len() != 1 {
        return Err(KainError::runtime(
            "kain_tensor_from_py: expected 1 argument (target)",
        ));
    }

    let state = python_scope_state(env)?;
    Python::with_gil(|py| {
        let scope = state.scope.read().unwrap();
        let scope_dict = scope_dict_from_guard(py, &scope)?;
        let target = resolve_python_target(py, scope_dict, &args[0])?;
        let metadata = resolve_payload_metadata(py, target.as_ref(py))?;
        build_native_tensor(target.as_ref(py), &metadata)
    })
}

fn kain_tensor_info_native(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    if args.len() != 1 {
        return Err(KainError::runtime(
            "kain_tensor_info: expected 1 argument (tensor)",
        ));
    }

    let tensor = extract_native_tensor(&args[0])?;
    Ok(native_tensor_info_value(&tensor))
}

fn kain_tensor_get_native(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    if args.len() != 2 {
        return Err(KainError::runtime(
            "kain_tensor_get: expected (tensor, indices)",
        ));
    }

    let tensor = extract_native_tensor(&args[0])?;
    let indices = parse_index_values("kain_tensor_get", &args[1])?;
    native_tensor_get(&tensor, &indices)
}

fn kain_tensor_set_native(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    if args.len() != 3 {
        return Err(KainError::runtime(
            "kain_tensor_set: expected (tensor, indices, value)",
        ));
    }

    let tensor = extract_native_tensor(&args[0])?;
    let indices = parse_index_values("kain_tensor_set", &args[1])?;
    native_tensor_set(&tensor, &indices, &args[2])?;
    Ok(Value::Unit)
}

fn kain_geometry_from_py_native(env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    if args.is_empty() || args.len() > 2 {
        return Err(KainError::runtime(
            "kain_geometry_from_py: expected (target) or (vertices, indices)",
        ));
    }

    let state = python_scope_state(env)?;
    Python::with_gil(|py| {
        let scope = state.scope.read().unwrap();
        let scope_dict = scope_dict_from_guard(py, &scope)?;
        let (vertices, indices) = resolve_geometry_targets(py, scope_dict, &args)?;
        let vertex_metadata = resolve_payload_metadata(py, vertices.as_ref(py))?;
        let index_metadata = indices
            .as_ref()
            .map(|value| resolve_payload_metadata(py, value.as_ref(py)))
            .transpose()?;
        let source = if args.len() == 1 {
            Some(resolve_python_target(py, scope_dict, &args[0])?)
        } else {
            None
        };
        build_native_geometry(
            vertices.as_ref(py),
            indices.as_ref().map(|value| value.as_ref(py)),
            &vertex_metadata,
            index_metadata.as_ref(),
            source.as_ref().map(|value| value.as_ref(py)),
        )
    })
}

fn kain_geometry_info_native(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    if args.len() != 1 {
        return Err(KainError::runtime(
            "kain_geometry_info: expected 1 argument (geometry)",
        ));
    }

    let geometry = extract_native_geometry(&args[0])?;
    Ok(native_geometry_info_value(&geometry))
}

fn kain_geometry_vertex_native(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    if args.len() != 2 {
        return Err(KainError::runtime(
            "kain_geometry_vertex: expected (geometry, index)",
        ));
    }

    let geometry = extract_native_geometry(&args[0])?;
    let index = value_to_int_arg("kain_geometry_vertex", "index", &args[1])?;
    native_geometry_vertex(&geometry, index)
}

fn kain_geometry_set_vertex_native(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    if args.len() != 3 {
        return Err(KainError::runtime(
            "kain_geometry_set_vertex: expected (geometry, index, value)",
        ));
    }

    let geometry = extract_native_geometry(&args[0])?;
    let index = value_to_int_arg("kain_geometry_set_vertex", "index", &args[1])?;
    native_geometry_set_vertex(&geometry, index, &args[2])?;
    Ok(Value::Unit)
}

fn kain_geometry_face_native(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    if args.len() != 2 {
        return Err(KainError::runtime(
            "kain_geometry_face: expected (geometry, index)",
        ));
    }

    let geometry = extract_native_geometry(&args[0])?;
    let index = value_to_int_arg("kain_geometry_face", "index", &args[1])?;
    native_geometry_face(&geometry, index)
}

fn kain_geometry_set_face_native(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    if args.len() != 3 {
        return Err(KainError::runtime(
            "kain_geometry_set_face: expected (geometry, index, value)",
        ));
    }

    let geometry = extract_native_geometry(&args[0])?;
    let index = value_to_int_arg("kain_geometry_set_face", "index", &args[1])?;
    native_geometry_set_face(&geometry, index, &args[2])?;
    Ok(Value::Unit)
}

fn kain_image_to_py_native(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let image = match args.as_slice() {
        [image] | [image, Value::String(_)] => extract_native_image(image)?,
        _ => {
            return Err(KainError::runtime(
                "kain_image_to_py: expected (image) or (image, backend)",
            ))
        }
    };
    let backend = parse_optional_backend_arg("kain_image_to_py", &args, "numpy")?;
    Python::with_gil(|py| wrap_python_object(export_native_image_pyobject(py, image.as_ref(), &backend)?.as_ref(py)))
}

fn kain_tensor_to_py_native(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let tensor = match args.as_slice() {
        [tensor] | [tensor, Value::String(_)] => extract_native_tensor(tensor)?,
        _ => {
            return Err(KainError::runtime(
                "kain_tensor_to_py: expected (tensor) or (tensor, backend)",
            ))
        }
    };
    let backend = parse_optional_backend_arg("kain_tensor_to_py", &args, "numpy")?;
    Python::with_gil(|py| wrap_python_object(export_native_tensor_pyobject(py, tensor.as_ref(), &backend)?.as_ref(py)))
}

fn kain_geometry_to_py_native(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let geometry = match args.as_slice() {
        [geometry] | [geometry, Value::String(_)] => extract_native_geometry(geometry)?,
        _ => {
            return Err(KainError::runtime(
                "kain_geometry_to_py: expected (geometry) or (geometry, backend)",
            ))
        }
    };
    let backend = parse_optional_backend_arg("kain_geometry_to_py", &args, "dict")?;
    Python::with_gil(|py| {
        wrap_python_object(export_native_geometry_pyobject(py, geometry.as_ref(), &backend)?.as_ref(py))
    })
}

fn parse_optional_backend_arg(fn_name: &str, args: &[Value], default: &str) -> KainResult<String> {
    match args {
        [_] => Ok(default.to_string()),
        [_, Value::String(backend)] => Ok(backend.clone()),
        [_, other] => Err(KainError::runtime(format!(
            "{fn_name}: expected backend to be a String, got {other:?}"
        ))),
        _ => Err(KainError::runtime(format!(
            "{fn_name}: invalid argument shape for backend parsing"
        ))),
    }
}

#[derive(Debug, Clone)]
struct PythonPayloadMetadata {
    backend: String,
    kind: String,
    label: String,
    dtype: String,
    format: Option<String>,
    shape: Vec<i64>,
    strides: Vec<i64>,
    ndim: i64,
    nbytes: i64,
    item_size: i64,
    readonly: bool,
    contiguous: bool,
    c_contiguous: bool,
    f_contiguous: bool,
    device: Option<String>,
    requires_grad: Option<bool>,
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

fn resolve_payload_metadata(py: Python<'_>, target: &PyAny) -> KainResult<PythonPayloadMetadata> {
    if is_torch_tensor(target) {
        return build_torch_tensor_metadata(py, target);
    }

    let view = create_memoryview(py, target)?;
    build_memoryview_metadata(target, view.as_ref(py))
}

fn build_memoryview_metadata(target: &PyAny, view: &PyAny) -> KainResult<PythonPayloadMetadata> {
    Ok(PythonPayloadMetadata {
        backend: detect_backend(target),
        kind: python_type_path(target),
        label: python_object_label(target),
        dtype: dtype_name_from_target_or_view(target, view)?,
        format: py_optional_string_attr_value(view, "format")?,
        shape: py_index_sequence_attr_values(view, "shape")?,
        strides: py_index_sequence_attr_values_or_default(view, "strides")?,
        ndim: py_int_attr_value(view, "ndim")?,
        nbytes: py_int_attr_value(view, "nbytes")?,
        item_size: py_int_attr_value(view, "itemsize")?,
        readonly: py_bool_attr_value(view, "readonly")?,
        contiguous: py_bool_attr_value(view, "contiguous")?,
        c_contiguous: py_bool_attr_value(view, "c_contiguous")?,
        f_contiguous: py_bool_attr_value(view, "f_contiguous")?,
        device: py_optional_string_attr_value(target, "device")?,
        requires_grad: py_optional_bool_attr_value(target, "requires_grad")?,
    })
}

fn build_torch_tensor_metadata(
    py: Python<'_>,
    target: &PyAny,
) -> KainResult<PythonPayloadMetadata> {
    let detached = target
        .call_method0("detach")
        .map_err(|err| KainError::runtime(format!("PyTorch detach error: {err}")))?;
    let cpu_tensor = detached
        .call_method0("cpu")
        .map_err(|err| KainError::runtime(format!("PyTorch cpu() error: {err}")))?;
    let contiguous_tensor = cpu_tensor
        .call_method0("contiguous")
        .map_err(|err| KainError::runtime(format!("PyTorch contiguous() error: {err}")))?;
    let numpy_array = contiguous_tensor
        .call_method0("numpy")
        .map_err(|err| KainError::runtime(format!("PyTorch numpy() error: {err}")))?;
    let view = create_memoryview(py, numpy_array)?;

    Ok(PythonPayloadMetadata {
        backend: "torch".to_string(),
        kind: python_type_path(target),
        label: python_object_label(target),
        dtype: torch_dtype_name(target)?,
        format: py_optional_string_attr_value(view.as_ref(py), "format")?,
        shape: py_index_sequence_values_from_object(target, "shape")?,
        strides: torch_stride_values(target)?,
        ndim: py_int_attr_value(target, "ndim")?,
        nbytes: torch_nbytes(target)?,
        item_size: torch_item_size(target)?,
        readonly: false,
        contiguous: torch_is_contiguous(target)?,
        c_contiguous: torch_is_contiguous(target)?,
        f_contiguous: false,
        device: py_optional_string_attr_value(target, "device")?,
        requires_grad: py_optional_bool_attr_value(target, "requires_grad")?,
    })
}

fn build_buffer_info(target: &PyAny, view: &PyAny) -> KainResult<Value> {
    let metadata = build_memoryview_metadata(target, view)?;
    Ok(metadata_to_value("PyBufferInfo", &metadata))
}

fn dtype_name_from_target_or_view(target: &PyAny, view: &PyAny) -> KainResult<String> {
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
                return Ok(name);
            }
            if let Ok(name) = dtype.str().and_then(|value| value.extract::<String>()) {
                return Ok(name);
            }
        }
    }

    Ok(py_optional_string_attr_value(view, "format")?.unwrap_or_else(|| "unknown".to_string()))
}

fn export_payload_bytes(py: Python<'_>, target: &PyAny) -> KainResult<PyObject> {
    if is_torch_tensor(target) {
        let contiguous_tensor = target
            .call_method0("detach")
            .and_then(|value| value.call_method0("cpu"))
            .and_then(|value| value.call_method0("contiguous"))
            .map_err(|err| KainError::runtime(format!("PyTorch tensor export error: {err}")))?;
        return contiguous_tensor
            .call_method0("numpy")
            .and_then(|value| value.call_method0("tobytes"))
            .map(|value| value.into_py(py))
            .map_err(|err| KainError::runtime(format!("PyTorch bytes export error: {err}")));
    }

    let view = create_memoryview(py, target)?;
    view.as_ref(py)
        .call_method0("tobytes")
        .map(|value| value.into_py(py))
        .map_err(|err| KainError::runtime(format!("Python buffer export error: {err}")))
}

fn metadata_to_value(name: &str, metadata: &PythonPayloadMetadata) -> Value {
    let mut fields = HashMap::new();
    fields.insert(
        "backend".to_string(),
        Value::String(metadata.backend.clone()),
    );
    fields.insert("kind".to_string(), Value::String(metadata.kind.clone()));
    fields.insert("label".to_string(), Value::String(metadata.label.clone()));
    fields.insert("dtype".to_string(), Value::String(metadata.dtype.clone()));
    fields.insert(
        "format".to_string(),
        optional_string_to_value(metadata.format.clone()),
    );
    fields.insert("shape".to_string(), int_list_to_value(&metadata.shape));
    fields.insert("strides".to_string(), int_list_to_value(&metadata.strides));
    fields.insert("ndim".to_string(), Value::Int(metadata.ndim));
    fields.insert("nbytes".to_string(), Value::Int(metadata.nbytes));
    fields.insert("item_size".to_string(), Value::Int(metadata.item_size));
    fields.insert("readonly".to_string(), Value::Bool(metadata.readonly));
    fields.insert("contiguous".to_string(), Value::Bool(metadata.contiguous));
    fields.insert(
        "c_contiguous".to_string(),
        Value::Bool(metadata.c_contiguous),
    );
    fields.insert(
        "f_contiguous".to_string(),
        Value::Bool(metadata.f_contiguous),
    );
    fields.insert(
        "device".to_string(),
        optional_string_to_value(metadata.device.clone()),
    );
    fields.insert(
        "requires_grad".to_string(),
        optional_bool_to_value(metadata.requires_grad),
    );
    Value::Struct(name.to_string(), Arc::new(RwLock::new(fields)))
}

fn build_image_info(metadata: &PythonPayloadMetadata) -> KainResult<Value> {
    let dims = &metadata.shape;
    let (layout, batch, height, width, channels) = match dims.as_slice() {
        [height, width] => ("HW".to_string(), 1, *height, *width, 1),
        [height, width, channels] if IMAGE_CHANNEL_COUNTS.contains(channels) => {
            ("HWC".to_string(), 1, *height, *width, *channels)
        }
        [channels, height, width] if IMAGE_CHANNEL_COUNTS.contains(channels) => {
            ("CHW".to_string(), 1, *height, *width, *channels)
        }
        [batch, height, width, channels] if IMAGE_CHANNEL_COUNTS.contains(channels) => {
            ("NHWC".to_string(), *batch, *height, *width, *channels)
        }
        [batch, channels, height, width] if IMAGE_CHANNEL_COUNTS.contains(channels) => {
            ("NCHW".to_string(), *batch, *height, *width, *channels)
        }
        _ => {
            return Err(KainError::runtime(format!(
                "py_image_info: cannot infer image layout from shape {:?}",
                dims
            )))
        }
    };

    let mut fields = struct_fields_from_value(metadata_to_value("PyImageInfo", metadata));
    fields.insert("layout".to_string(), Value::String(layout));
    fields.insert("batch".to_string(), Value::Int(batch));
    fields.insert("height".to_string(), Value::Int(height));
    fields.insert("width".to_string(), Value::Int(width));
    fields.insert("channels".to_string(), Value::Int(channels));
    fields.insert(
        "pixel_count".to_string(),
        Value::Int(height.saturating_mul(width).saturating_mul(batch)),
    );
    fields.insert(
        "channel_last".to_string(),
        Value::Bool(
            matches!(dims.len(), 2)
                || matches!(dims.as_slice(), [_, _, c] if IMAGE_CHANNEL_COUNTS.contains(c))
                || matches!(dims.as_slice(), [_, _, _, c] if IMAGE_CHANNEL_COUNTS.contains(c)),
        ),
    );
    Ok(Value::Struct(
        "PyImageInfo".to_string(),
        Arc::new(RwLock::new(fields)),
    ))
}

fn build_image_view(target: PyObject, metadata: &PythonPayloadMetadata) -> KainResult<Value> {
    let info = build_image_info(metadata)?;
    let fields = struct_fields_from_value(info);
    let layout = struct_string_field(&fields, "layout")?;
    let batch = struct_int_field(&fields, "batch")?;
    let width = struct_int_field(&fields, "width")?;
    let height = struct_int_field(&fields, "height")?;
    let channels = struct_int_field(&fields, "channels")?;

    Ok(Value::host_object(
        "python:view:image",
        Arc::new(PythonImageView {
            object: target,
            layout,
            batch,
            width,
            height,
            channels,
        }),
    ))
}

fn build_geometry_info(
    vertices: &PythonPayloadMetadata,
    indices: Option<&PythonPayloadMetadata>,
) -> KainResult<Value> {
    let vertex_shape = vertices.shape.as_slice();
    let (vertex_count, components) = match vertex_shape {
        [count, components] if (2..=4).contains(components) => (*count, *components),
        _ => {
            return Err(KainError::runtime(format!(
                "py_geometry_info: expected vertex shape [N, 2|3|4], found {:?}",
                vertices.shape
            )))
        }
    };

    let (index_count, face_count, face_size) = match indices {
        Some(indices) => match indices.shape.as_slice() {
            [count] => (*count, *count, 1),
            [count, face_size] if (2..=4).contains(face_size) => {
                (count.saturating_mul(*face_size), *count, *face_size)
            }
            _ => {
                return Err(KainError::runtime(format!(
                    "py_geometry_info: expected index shape [M] or [M, 2|3|4], found {:?}",
                    indices.shape
                )))
            }
        },
        None => (0, 0, 0),
    };

    let mut fields = HashMap::new();
    fields.insert(
        "backend".to_string(),
        Value::String(vertices.backend.clone()),
    );
    fields.insert(
        "vertex_dtype".to_string(),
        Value::String(vertices.dtype.clone()),
    );
    fields.insert("vertex_count".to_string(), Value::Int(vertex_count));
    fields.insert("components".to_string(), Value::Int(components));
    fields.insert(
        "vertex_shape".to_string(),
        int_list_to_value(&vertices.shape),
    );
    fields.insert(
        "vertex_stride".to_string(),
        Value::Int(
            vertices
                .strides
                .last()
                .copied()
                .unwrap_or(vertices.item_size),
        ),
    );
    fields.insert("indexed".to_string(), Value::Bool(indices.is_some()));
    fields.insert("index_count".to_string(), Value::Int(index_count));
    fields.insert("face_count".to_string(), Value::Int(face_count));
    fields.insert("face_size".to_string(), Value::Int(face_size));
    fields.insert(
        "primitive".to_string(),
        Value::String(if indices.is_some() {
            "mesh".to_string()
        } else {
            "point_cloud".to_string()
        }),
    );

    if let Some(indices) = indices {
        fields.insert(
            "index_dtype".to_string(),
            Value::String(indices.dtype.clone()),
        );
        fields.insert("index_shape".to_string(), int_list_to_value(&indices.shape));
    } else {
        fields.insert("index_dtype".to_string(), Value::None);
        fields.insert("index_shape".to_string(), Value::None);
    }

    Ok(Value::Struct(
        "PyGeometryInfo".to_string(),
        Arc::new(RwLock::new(fields)),
    ))
}

fn build_geometry_view(
    vertices: PyObject,
    indices: Option<PyObject>,
    vertex_metadata: PythonPayloadMetadata,
    index_metadata: Option<PythonPayloadMetadata>,
) -> KainResult<Value> {
    let info = build_geometry_info(&vertex_metadata, index_metadata.as_ref())?;
    let fields = struct_fields_from_value(info);
    let components = struct_int_field(&fields, "components")?;
    let face_size = struct_int_field(&fields, "face_size")?;

    Ok(Value::host_object(
        "python:view:geometry",
        Arc::new(PythonGeometryView {
            vertices,
            indices,
            vertex_metadata,
            index_metadata,
            components,
            face_size,
        }),
    ))
}

fn resolve_geometry_targets<'py>(
    py: Python<'py>,
    scope: &'py PyDict,
    args: &[Value],
) -> KainResult<(PyObject, Option<PyObject>)> {
    if args.len() == 1 {
        if let Some(view) = args[0].downcast_host_object::<PythonGeometryView>() {
            return Ok((
                view.vertices.clone_ref(py),
                view.indices.as_ref().map(|value| value.clone_ref(py)),
            ));
        }
    }

    if args.len() == 2 {
        let vertices = resolve_python_target(py, scope, &args[0])?;
        let indices = resolve_python_target(py, scope, &args[1])?;
        return Ok((vertices, Some(indices)));
    }

    let target = resolve_python_target(py, scope, &args[0])?;
    let target_ref = target.as_ref(py);
    if let Ok(vertices) = target_ref.getattr("vertices") {
        let indices = target_ref
            .getattr("faces")
            .ok()
            .map(|value| value.into_py(py));
        return Ok((vertices.into_py(py), indices));
    }

    Ok((target, None))
}

fn py_optional_string_attr_value(target: &PyAny, name: &str) -> KainResult<Option<String>> {
    match target.getattr(name) {
        Ok(value) if value.is_none() => Ok(None),
        Ok(value) => match value.extract::<String>() {
            Ok(text) => Ok(Some(text)),
            Err(_) => value
                .str()
                .and_then(|text| text.extract::<String>())
                .map(Some)
                .map_err(|err| {
                    KainError::runtime(format!("Python attribute error ({name}): {err}"))
                }),
        },
        Err(_) => Ok(None),
    }
}

fn py_optional_bool_attr_value(target: &PyAny, name: &str) -> KainResult<Option<bool>> {
    match target.getattr(name) {
        Ok(value) if value.is_none() => Ok(None),
        Ok(value) => value
            .extract::<bool>()
            .map(Some)
            .map_err(|err| KainError::runtime(format!("Python attribute error ({name}): {err}"))),
        Err(_) => Ok(None),
    }
}

fn py_int_attr_value(target: &PyAny, name: &str) -> KainResult<i64> {
    target
        .getattr(name)
        .and_then(|value| value.extract::<i64>())
        .map_err(|err| KainError::runtime(format!("Python attribute error ({name}): {err}")))
}

fn py_bool_attr_value(target: &PyAny, name: &str) -> KainResult<bool> {
    target
        .getattr(name)
        .and_then(|value| value.extract::<bool>())
        .map_err(|err| KainError::runtime(format!("Python attribute error ({name}): {err}")))
}

fn py_index_sequence_attr_values(target: &PyAny, name: &str) -> KainResult<Vec<i64>> {
    let value = target
        .getattr(name)
        .map_err(|err| KainError::runtime(format!("Python attribute error ({name}): {err}")))?;
    py_index_sequence_values(value, name)
}

fn py_index_sequence_values_from_object(target: &PyAny, name: &str) -> KainResult<Vec<i64>> {
    let value = target
        .getattr(name)
        .map_err(|err| KainError::runtime(format!("Python attribute error ({name}): {err}")))?;
    py_index_sequence_values(value, name)
}

fn py_index_sequence_attr_values_or_default(target: &PyAny, name: &str) -> KainResult<Vec<i64>> {
    match target.getattr(name) {
        Ok(value) if value.is_none() => Ok(Vec::new()),
        Ok(value) => py_index_sequence_values(value, name),
        Err(_) => Ok(Vec::new()),
    }
}

fn py_index_sequence_values(value: &PyAny, name: &str) -> KainResult<Vec<i64>> {
    if value.is_none() {
        return Ok(Vec::new());
    }
    if let Ok(tuple) = value.downcast::<PyTuple>() {
        return tuple
            .iter()
            .map(|item| {
                item.extract::<i64>().map_err(|err| {
                    KainError::runtime(format!("Python sequence conversion error ({name}): {err}"))
                })
            })
            .collect();
    }
    if let Ok(list) = value.downcast::<PyList>() {
        return list
            .iter()
            .map(|item| {
                item.extract::<i64>().map_err(|err| {
                    KainError::runtime(format!("Python sequence conversion error ({name}): {err}"))
                })
            })
            .collect();
    }
    Ok(vec![value.extract::<i64>().map_err(|err| {
        KainError::runtime(format!("Python sequence conversion error ({name}): {err}"))
    })?])
}

fn int_list_to_value(values: &[i64]) -> Value {
    Value::Array(Arc::new(RwLock::new(
        values.iter().copied().map(Value::Int).collect(),
    )))
}

fn optional_string_to_value(value: Option<String>) -> Value {
    value.map(Value::String).unwrap_or(Value::None)
}

fn optional_bool_to_value(value: Option<bool>) -> Value {
    value.map(Value::Bool).unwrap_or(Value::None)
}

fn struct_fields_from_value(value: Value) -> HashMap<String, Value> {
    match value {
        Value::Struct(_, fields) => fields.read().unwrap().clone(),
        _ => HashMap::new(),
    }
}

fn struct_string_field(fields: &HashMap<String, Value>, name: &str) -> KainResult<String> {
    match fields.get(name) {
        Some(Value::String(value)) => Ok(value.clone()),
        Some(other) => Err(KainError::runtime(format!(
            "Expected struct field {name} to be String, got {other:?}"
        ))),
        None => Err(KainError::runtime(format!(
            "Missing struct field {name} in typed Python view info"
        ))),
    }
}

fn struct_int_field(fields: &HashMap<String, Value>, name: &str) -> KainResult<i64> {
    match fields.get(name) {
        Some(Value::Int(value)) => Ok(*value),
        Some(other) => Err(KainError::runtime(format!(
            "Expected struct field {name} to be Int, got {other:?}"
        ))),
        None => Err(KainError::runtime(format!(
            "Missing struct field {name} in typed Python view info"
        ))),
    }
}

fn value_to_int_arg(fn_name: &str, arg_name: &str, value: &Value) -> KainResult<i64> {
    match value {
        Value::Int(value) => Ok(*value),
        other => Err(KainError::runtime(format!(
            "{fn_name}: expected integer {arg_name}, got {other:?}"
        ))),
    }
}

fn parse_index_values(fn_name: &str, value: &Value) -> KainResult<Vec<i64>> {
    match value {
        Value::Int(value) => Ok(vec![*value]),
        Value::Array(values) => values
            .read()
            .unwrap()
            .iter()
            .map(|item| value_to_int_arg(fn_name, "indices", item))
            .collect(),
        Value::Tuple(values) => values
            .iter()
            .map(|item| value_to_int_arg(fn_name, "indices", item))
            .collect(),
        other => Err(KainError::runtime(format!(
            "{fn_name}: expected integer index or index tuple/array, got {other:?}"
        ))),
    }
}

fn validate_rank(fn_name: &str, shape: &[i64], index_count: usize, label: &str) -> KainResult<()> {
    if !shape.is_empty() && index_count > shape.len() {
        return Err(KainError::runtime(format!(
            "{fn_name}: expected at most {} indices for {label}, got {index_count}",
            shape.len()
        )));
    }
    Ok(())
}

fn validate_axis_index(fn_name: &str, index: i64, len: i64) -> KainResult<()> {
    if index < 0 || index >= len {
        return Err(KainError::runtime(format!(
            "{fn_name}: index {index} is outside 0..{}",
            len.saturating_sub(1)
        )));
    }
    Ok(())
}

fn normalize_vector_value(value: Value) -> Value {
    match value {
        Value::Array(_) => value,
        Value::Tuple(items) => Value::Array(Arc::new(RwLock::new(items))),
        other => Value::Array(Arc::new(RwLock::new(vec![other]))),
    }
}

fn validate_vector_length(fn_name: &str, value: &Value, expected_len: i64) -> KainResult<()> {
    if expected_len <= 0 {
        return Ok(());
    }

    let actual_len = match value {
        Value::Array(values) => values.read().unwrap().len() as i64,
        Value::Tuple(values) => values.len() as i64,
        _ => 1,
    };

    if actual_len != expected_len {
        return Err(KainError::runtime(format!(
            "{fn_name}: expected vector length {expected_len}, got {actual_len}"
        )));
    }
    Ok(())
}

fn extract_image_view(value: &Value) -> KainResult<Arc<PythonImageView>> {
    value
        .downcast_host_object::<PythonImageView>()
        .ok_or_else(|| {
            KainError::runtime(format!(
                "Expected typed Python image view, got {}",
                value.host_object_label().unwrap_or("value")
            ))
        })
}

fn extract_tensor_view(value: &Value) -> KainResult<Arc<PythonTensorView>> {
    value
        .downcast_host_object::<PythonTensorView>()
        .ok_or_else(|| {
            KainError::runtime(format!(
                "Expected typed Python tensor view, got {}",
                value.host_object_label().unwrap_or("value")
            ))
        })
}

fn extract_geometry_view(value: &Value) -> KainResult<Arc<PythonGeometryView>> {
    value
        .downcast_host_object::<PythonGeometryView>()
        .ok_or_else(|| {
            KainError::runtime(format!(
                "Expected typed Python geometry view, got {}",
                value.host_object_label().unwrap_or("value")
            ))
        })
}

fn image_pixel_indices(
    py: Python<'_>,
    view: &PythonImageView,
    batch: i64,
    x: i64,
    y: i64,
) -> KainResult<Vec<PyObject>> {
    if batch < 0 || batch >= view.batch {
        return Err(KainError::runtime(format!(
            "py_image_pixel: batch index {batch} is outside 0..{}",
            view.batch.saturating_sub(1)
        )));
    }
    if x < 0 || x >= view.width {
        return Err(KainError::runtime(format!(
            "py_image_pixel: x index {x} is outside 0..{}",
            view.width.saturating_sub(1)
        )));
    }
    if y < 0 || y >= view.height {
        return Err(KainError::runtime(format!(
            "py_image_pixel: y index {y} is outside 0..{}",
            view.height.saturating_sub(1)
        )));
    }

    let full = python_full_slice(py)?;
    let indices = match view.layout.as_str() {
        "HW" | "HWC" => vec![y.into_py(py), x.into_py(py)],
        "CHW" => vec![full, y.into_py(py), x.into_py(py)],
        "NHWC" => vec![batch.into_py(py), y.into_py(py), x.into_py(py)],
        "NCHW" => vec![batch.into_py(py), full, y.into_py(py), x.into_py(py)],
        other => {
            return Err(KainError::runtime(format!(
                "py_image_pixel: unsupported image layout {other}"
            )))
        }
    };
    Ok(indices)
}

fn python_full_slice(py: Python<'_>) -> KainResult<PyObject> {
    let builtins = py
        .import("builtins")
        .map_err(|err| KainError::runtime(format!("Python import error: {err}")))?;
    builtins
        .getattr("slice")
        .and_then(|callable| callable.call1((py.None(),)))
        .map(|value| value.into_py(py))
        .map_err(|err| KainError::runtime(format!("Python slice error: {err}")))
}

fn get_python_item(py: Python<'_>, target: &PyAny, indices: &[PyObject]) -> KainResult<PyObject> {
    if indices.len() == 1 {
        return target
            .get_item(indices[0].clone_ref(py))
            .map(|value| value.into_py(py))
            .map_err(|err| KainError::runtime(format!("Python indexing error: {err}")));
    }

    let index_tuple = PyTuple::new(py, indices.iter().map(|value| value.clone_ref(py)));
    target
        .get_item(index_tuple)
        .map(|value| value.into_py(py))
        .map_err(|err| KainError::runtime(format!("Python indexing error: {err}")))
}

fn set_python_item(
    py: Python<'_>,
    target: &PyAny,
    indices: &[PyObject],
    value: PyObject,
) -> KainResult<()> {
    if indices.len() == 1 {
        return target
            .set_item(indices[0].clone_ref(py), value)
            .map_err(|err| KainError::runtime(format!("Python assignment error: {err}")));
    }

    let index_tuple = PyTuple::new(py, indices.iter().map(|index| index.clone_ref(py)));
    target
        .set_item(index_tuple, value)
        .map_err(|err| KainError::runtime(format!("Python assignment error: {err}")))
}

impl NativeScalarBuffer {
    fn scalar_kind(&self) -> &'static str {
        match self {
            Self::Bool(_) => "bool",
            Self::Int(_) => "int",
            Self::Float(_) => "float",
            Self::Shared(shared) => match shared.kind {
                SharedScalarKind::Bool => "bool",
                SharedScalarKind::Int => "int",
                SharedScalarKind::Float => "float",
            },
        }
    }

    fn len(&self) -> usize {
        match self {
            Self::Bool(values) => values.read().unwrap().len(),
            Self::Int(values) => values.read().unwrap().len(),
            Self::Float(values) => values.read().unwrap().len(),
            Self::Shared(shared) => shared.len,
        }
    }

    fn get_value(&self, index: usize) -> KainResult<Value> {
        match self {
            Self::Bool(values) => values
                .read()
                .unwrap()
                .get(index)
                .copied()
                .map(Value::Bool)
                .ok_or_else(|| {
                    KainError::runtime(format!("native buffer index {index} is out of bounds"))
                }),
            Self::Int(values) => values
                .read()
                .unwrap()
                .get(index)
                .copied()
                .map(Value::Int)
                .ok_or_else(|| {
                    KainError::runtime(format!("native buffer index {index} is out of bounds"))
                }),
            Self::Float(values) => values
                .read()
                .unwrap()
                .get(index)
                .copied()
                .map(Value::Float)
                .ok_or_else(|| {
                    KainError::runtime(format!("native buffer index {index} is out of bounds"))
                }),
            Self::Shared(shared) => shared_buffer_get_value(shared.as_ref(), index),
        }
    }

    fn set_value(&self, index: usize, value: &Value) -> KainResult<()> {
        match self {
            Self::Bool(values) => {
                let converted = match value {
                    Value::Bool(value) => *value,
                    Value::Int(value) => *value != 0,
                    other => {
                        return Err(KainError::runtime(format!(
                            "Expected bool-compatible value, got {other:?}"
                        )))
                    }
                };
                let mut values = values.write().unwrap();
                let slot = values.get_mut(index).ok_or_else(|| {
                    KainError::runtime(format!("native buffer index {index} is out of bounds"))
                })?;
                *slot = converted;
                Ok(())
            }
            Self::Int(values) => {
                let converted = match value {
                    Value::Int(value) => *value,
                    Value::Bool(value) => i64::from(*value),
                    other => {
                        return Err(KainError::runtime(format!(
                            "Expected int-compatible value, got {other:?}"
                        )))
                    }
                };
                let mut values = values.write().unwrap();
                let slot = values.get_mut(index).ok_or_else(|| {
                    KainError::runtime(format!("native buffer index {index} is out of bounds"))
                })?;
                *slot = converted;
                Ok(())
            }
            Self::Float(values) => {
                let converted = match value {
                    Value::Float(value) => *value,
                    Value::Int(value) => *value as f64,
                    Value::Bool(value) => {
                        if *value {
                            1.0
                        } else {
                            0.0
                        }
                    }
                    other => {
                        return Err(KainError::runtime(format!(
                            "Expected float-compatible value, got {other:?}"
                        )))
                    }
                };
                let mut values = values.write().unwrap();
                let slot = values.get_mut(index).ok_or_else(|| {
                    KainError::runtime(format!("native buffer index {index} is out of bounds"))
                })?;
                *slot = converted;
                Ok(())
            }
            Self::Shared(shared) => shared_buffer_set_value(shared.as_ref(), index, value),
        }
    }

    fn is_shared(&self) -> bool {
        matches!(self, Self::Shared(_))
    }

    fn shared_owner(&self, py: Python<'_>) -> Option<PyObject> {
        match self {
            Self::Shared(shared) => Some(shared.owner.clone_ref(py)),
            _ => None,
        }
    }

    fn shared_backend(&self) -> Option<&str> {
        match self {
            Self::Shared(shared) => Some(shared.backend.as_str()),
            _ => None,
        }
    }
}

fn try_build_shared_native_scalar_buffer(
    py: Python<'_>,
    target: &PyAny,
    metadata: &PythonPayloadMetadata,
) -> KainResult<Option<NativeScalarBuffer>> {
    if !metadata.c_contiguous || metadata.readonly {
        return Ok(None);
    }

    let Some(kind) = shared_scalar_kind_for_dtype(&metadata.dtype) else {
        return Ok(None);
    };
    let len = checked_element_count(&metadata.shape)?;

    match metadata.backend.as_str() {
        "numpy" => {
            let flat = target
                .call_method1("reshape", (-1,))
                .map_err(|err| KainError::runtime(format!("NumPy shared reshape error: {err}")))?;
            Ok(Some(NativeScalarBuffer::Shared(Arc::new(SharedPythonBuffer {
                owner: target.into_py(py),
                flat: flat.into_py(py),
                backend: "numpy".to_string(),
                kind,
                len,
            }))))
        }
        "torch" => {
            let device = metadata.device.as_deref().unwrap_or("cpu");
            if device != "cpu" {
                return Ok(None);
            }
            let detached = target
                .call_method0("detach")
                .map_err(|err| KainError::runtime(format!("PyTorch shared detach error: {err}")))?;
            let flat = detached
                .call_method1("reshape", (-1,))
                .map_err(|err| KainError::runtime(format!("PyTorch shared reshape error: {err}")))?;
            Ok(Some(NativeScalarBuffer::Shared(Arc::new(SharedPythonBuffer {
                owner: target.into_py(py),
                flat: flat.into_py(py),
                backend: "torch".to_string(),
                kind,
                len,
            }))))
        }
        _ => Ok(None),
    }
}

fn shared_scalar_kind_for_dtype(dtype: &str) -> Option<SharedScalarKind> {
    match dtype {
        "bool" | "bool_" => Some(SharedScalarKind::Bool),
        "uint8" | "ubyte" | "int8" | "byte" | "uint16" | "int16" | "uint32" | "int32"
        | "uint64" | "int64" => Some(SharedScalarKind::Int),
        "float32" | "float64" | "double" => Some(SharedScalarKind::Float),
        _ => None,
    }
}

fn shared_buffer_get_value(shared: &SharedPythonBuffer, index: usize) -> KainResult<Value> {
    if index >= shared.len {
        return Err(KainError::runtime(format!(
            "native buffer index {index} is out of bounds"
        )));
    }
    Python::with_gil(|py| {
        let item = shared
            .flat
            .as_ref(py)
            .get_item(index)
            .map_err(|err| KainError::runtime(format!("Shared Python buffer indexing error: {err}")))?;
        py_any_to_value(item)
    })
}

fn shared_buffer_set_value(
    shared: &SharedPythonBuffer,
    index: usize,
    value: &Value,
) -> KainResult<()> {
    if index >= shared.len {
        return Err(KainError::runtime(format!(
            "native buffer index {index} is out of bounds"
        )));
    }

    Python::with_gil(|py| {
        let converted = match shared.kind {
            SharedScalarKind::Bool => match value {
                Value::Bool(value) => (*value).into_py(py),
                Value::Int(value) => (*value != 0).into_py(py),
                other => {
                    return Err(KainError::runtime(format!(
                        "Expected bool-compatible value, got {other:?}"
                    )))
                }
            },
            SharedScalarKind::Int => match value {
                Value::Int(value) => (*value).into_py(py),
                Value::Bool(value) => i64::from(*value).into_py(py),
                other => {
                    return Err(KainError::runtime(format!(
                        "Expected int-compatible value, got {other:?}"
                    )))
                }
            },
            SharedScalarKind::Float => match value {
                Value::Float(value) => (*value).into_py(py),
                Value::Int(value) => (*value as f64).into_py(py),
                Value::Bool(value) => {
                    if *value {
                        1.0f64.into_py(py)
                    } else {
                        0.0f64.into_py(py)
                    }
                }
                other => {
                    return Err(KainError::runtime(format!(
                        "Expected float-compatible value, got {other:?}"
                    )))
                }
            },
        };

        shared
            .flat
            .as_ref(py)
            .set_item(index, converted)
            .map_err(|err| KainError::runtime(format!("Shared Python buffer assignment error: {err}")))
    })
}

fn build_native_image(target: &PyAny, metadata: &PythonPayloadMetadata) -> KainResult<Value> {
    let info = build_image_info(metadata)?;
    let fields = struct_fields_from_value(info);
    let layout = struct_string_field(&fields, "layout")?;
    let batch = struct_int_field(&fields, "batch")?;
    let width = struct_int_field(&fields, "width")?;
    let height = struct_int_field(&fields, "height")?;
    let channels = struct_int_field(&fields, "channels")?;
    let data = try_build_shared_native_scalar_buffer(target.py(), target, metadata)?
        .unwrap_or(decode_native_scalar_buffer(
            export_payload_bytes(target.py(), target)?.as_ref(target.py()),
            &metadata.dtype,
        )?);
    let expected_len = checked_element_count(&metadata.shape)?;
    if data.len() != expected_len {
        return Err(KainError::runtime(format!(
            "kain_image_from_py: decoded {} values but expected {expected_len}",
            data.len()
        )));
    }

    Ok(Value::host_object(
        "kain:image",
        Arc::new(KainNativeImage {
            dtype: metadata.dtype.clone(),
            shape: metadata.shape.clone(),
            layout,
            batch,
            width,
            height,
            channels,
            data,
            source: Some(target.into_py(target.py())),
        }),
    ))
}

fn build_native_tensor(target: &PyAny, metadata: &PythonPayloadMetadata) -> KainResult<Value> {
    let data = try_build_shared_native_scalar_buffer(target.py(), target, metadata)?
        .unwrap_or(decode_native_scalar_buffer(
            export_payload_bytes(target.py(), target)?.as_ref(target.py()),
            &metadata.dtype,
        )?);
    let expected_len = checked_element_count(&metadata.shape)?;
    if data.len() != expected_len {
        return Err(KainError::runtime(format!(
            "kain_tensor_from_py: decoded {} values but expected {expected_len}",
            data.len()
        )));
    }

    Ok(Value::host_object(
        "kain:tensor",
        Arc::new(KainNativeTensor {
            dtype: metadata.dtype.clone(),
            shape: metadata.shape.clone(),
            data,
            source: Some(target.into_py(target.py())),
        }),
    ))
}

fn build_native_geometry(
    vertices: &PyAny,
    indices: Option<&PyAny>,
    vertex_metadata: &PythonPayloadMetadata,
    index_metadata: Option<&PythonPayloadMetadata>,
    source: Option<&PyAny>,
) -> KainResult<Value> {
    let info = build_geometry_info(vertex_metadata, index_metadata)?;
    let fields = struct_fields_from_value(info);
    let components = struct_int_field(&fields, "components")?;
    let face_size = struct_int_field(&fields, "face_size")?;

    let vertex_buffer = try_build_shared_native_scalar_buffer(vertices.py(), vertices, vertex_metadata)?
        .unwrap_or(decode_native_scalar_buffer(
            export_payload_bytes(vertices.py(), vertices)?.as_ref(vertices.py()),
            &vertex_metadata.dtype,
        )?);
    let expected_vertex_len = checked_element_count(&vertex_metadata.shape)?;
    if vertex_buffer.len() != expected_vertex_len {
        return Err(KainError::runtime(format!(
            "kain_geometry_from_py: decoded {} vertex values but expected {expected_vertex_len}",
            vertex_buffer.len()
        )));
    }

    let index_buffer = match (indices, index_metadata) {
        (Some(indices), Some(metadata)) => {
            let buffer = try_build_shared_native_scalar_buffer(indices.py(), indices, metadata)?
                .unwrap_or(decode_native_scalar_buffer(
                    export_payload_bytes(indices.py(), indices)?.as_ref(indices.py()),
                    &metadata.dtype,
                )?);
            let expected_index_len = checked_element_count(&metadata.shape)?;
            if buffer.len() != expected_index_len {
                return Err(KainError::runtime(format!(
                    "kain_geometry_from_py: decoded {} index values but expected {expected_index_len}",
                    buffer.len()
                )));
            }
            Some(buffer)
        }
        _ => None,
    };

    Ok(Value::host_object(
        "kain:geometry",
        Arc::new(KainNativeGeometry {
            vertex_dtype: vertex_metadata.dtype.clone(),
            vertex_shape: vertex_metadata.shape.clone(),
            components,
            vertices: vertex_buffer,
            index_dtype: index_metadata.map(|metadata| metadata.dtype.clone()),
            index_shape: index_metadata
                .map(|metadata| metadata.shape.clone())
                .unwrap_or_default(),
            face_size,
            indices: index_buffer,
            source: source.map(|value| value.into_py(value.py())),
        }),
    ))
}

fn native_image_info_value(image: &KainNativeImage) -> Value {
    let mut fields = HashMap::new();
    fields.insert("dtype".to_string(), Value::String(image.dtype.clone()));
    fields.insert("shape".to_string(), int_list_to_value(&image.shape));
    fields.insert("layout".to_string(), Value::String(image.layout.clone()));
    fields.insert("batch".to_string(), Value::Int(image.batch));
    fields.insert("width".to_string(), Value::Int(image.width));
    fields.insert("height".to_string(), Value::Int(image.height));
    fields.insert("channels".to_string(), Value::Int(image.channels));
    fields.insert(
        "storage".to_string(),
        Value::String(image.data.scalar_kind().to_string()),
    );
    fields.insert("zero_copy".to_string(), Value::Bool(image.data.is_shared()));
    fields.insert(
        "source_backend".to_string(),
        image.data
            .shared_backend()
            .map(|value| Value::String(value.to_string()))
            .unwrap_or(Value::None),
    );
    fields.insert(
        "pixel_count".to_string(),
        Value::Int(image.width * image.height * image.batch),
    );
    Value::Struct("KainImageInfo".to_string(), Arc::new(RwLock::new(fields)))
}

fn native_tensor_info_value(tensor: &KainNativeTensor) -> Value {
    let mut fields = HashMap::new();
    fields.insert("dtype".to_string(), Value::String(tensor.dtype.clone()));
    fields.insert("shape".to_string(), int_list_to_value(&tensor.shape));
    fields.insert(
        "storage".to_string(),
        Value::String(tensor.data.scalar_kind().to_string()),
    );
    fields.insert("zero_copy".to_string(), Value::Bool(tensor.data.is_shared()));
    fields.insert(
        "source_backend".to_string(),
        tensor
            .data
            .shared_backend()
            .map(|value| Value::String(value.to_string()))
            .unwrap_or(Value::None),
    );
    fields.insert("length".to_string(), Value::Int(tensor.data.len() as i64));
    Value::Struct("KainTensorInfo".to_string(), Arc::new(RwLock::new(fields)))
}

fn native_geometry_info_value(geometry: &KainNativeGeometry) -> Value {
    let mut fields = HashMap::new();
    let vertex_count = geometry.vertex_shape.first().copied().unwrap_or(0);
    let face_count = geometry.index_shape.first().copied().unwrap_or(0);
    fields.insert(
        "vertex_dtype".to_string(),
        Value::String(geometry.vertex_dtype.clone()),
    );
    fields.insert(
        "vertex_shape".to_string(),
        int_list_to_value(&geometry.vertex_shape),
    );
    fields.insert("vertex_count".to_string(), Value::Int(vertex_count));
    fields.insert("components".to_string(), Value::Int(geometry.components));
    fields.insert("face_count".to_string(), Value::Int(face_count));
    fields.insert("face_size".to_string(), Value::Int(geometry.face_size));
    fields.insert(
        "indexed".to_string(),
        Value::Bool(geometry.indices.is_some()),
    );
    fields.insert(
        "shared_vertices".to_string(),
        Value::Bool(geometry.vertices.is_shared()),
    );
    fields.insert(
        "shared_indices".to_string(),
        Value::Bool(
            geometry
                .indices
                .as_ref()
                .map(|value| value.is_shared())
                .unwrap_or(false),
        ),
    );
    fields.insert(
        "index_dtype".to_string(),
        geometry
            .index_dtype
            .as_ref()
            .cloned()
            .map(Value::String)
            .unwrap_or(Value::None),
    );
    fields.insert(
        "index_shape".to_string(),
        if geometry.index_shape.is_empty() {
            Value::None
        } else {
            int_list_to_value(&geometry.index_shape)
        },
    );
    Value::Struct(
        "KainGeometryInfo".to_string(),
        Arc::new(RwLock::new(fields)),
    )
}

fn native_image_pixel(image: &KainNativeImage, batch: i64, x: i64, y: i64) -> KainResult<Value> {
    let indices = native_image_channel_indices(image, batch, x, y)?;
    let mut values = Vec::with_capacity(indices.len());
    for index in indices {
        values.push(image.data.get_value(index)?);
    }
    Ok(Value::Array(Arc::new(RwLock::new(values))))
}

fn native_image_set_pixel(
    image: &KainNativeImage,
    batch: i64,
    x: i64,
    y: i64,
    value: &Value,
) -> KainResult<()> {
    validate_pixel_value("kain_image_set_pixel", value, image.channels)?;
    let indices = native_image_channel_indices(image, batch, x, y)?;
    let values = collect_vector_values(value, image.channels)?;
    for (slot, value) in indices.iter().zip(values.iter()) {
        image.data.set_value(*slot, value)?;
    }
    Ok(())
}

fn native_tensor_get(tensor: &KainNativeTensor, indices: &[i64]) -> KainResult<Value> {
    let index = flatten_row_major_index("kain_tensor_get", &tensor.shape, indices)?;
    tensor.data.get_value(index)
}

fn native_tensor_set(tensor: &KainNativeTensor, indices: &[i64], value: &Value) -> KainResult<()> {
    let index = flatten_row_major_index("kain_tensor_set", &tensor.shape, indices)?;
    tensor.data.set_value(index, value)
}

fn native_geometry_vertex(geometry: &KainNativeGeometry, index: i64) -> KainResult<Value> {
    let vertex_count = geometry.vertex_shape.first().copied().unwrap_or(0);
    validate_axis_index("kain_geometry_vertex", index, vertex_count)?;
    let start = usize::try_from(index)
        .map_err(|_| KainError::runtime(format!("kain_geometry_vertex: invalid index {index}")))?
        * usize::try_from(geometry.components).map_err(|_| {
            KainError::runtime(format!(
                "kain_geometry_vertex: invalid component count {}",
                geometry.components
            ))
        })?;
    native_vector_from_buffer(&geometry.vertices, start, geometry.components)
}

fn native_geometry_set_vertex(
    geometry: &KainNativeGeometry,
    index: i64,
    value: &Value,
) -> KainResult<()> {
    let vertex_count = geometry.vertex_shape.first().copied().unwrap_or(0);
    validate_axis_index("kain_geometry_set_vertex", index, vertex_count)?;
    validate_vector_value("kain_geometry_set_vertex", value, geometry.components)?;
    let start = usize::try_from(index).map_err(|_| {
        KainError::runtime(format!("kain_geometry_set_vertex: invalid index {index}"))
    })? * usize::try_from(geometry.components).map_err(|_| {
        KainError::runtime(format!(
            "kain_geometry_set_vertex: invalid component count {}",
            geometry.components
        ))
    })?;
    native_vector_write(&geometry.vertices, start, geometry.components, value)
}

fn native_geometry_face(geometry: &KainNativeGeometry, index: i64) -> KainResult<Value> {
    let Some(indices) = geometry.indices.as_ref() else {
        return Err(KainError::runtime(
            "kain_geometry_face: geometry has no index buffer",
        ));
    };
    let face_count = geometry.index_shape.first().copied().unwrap_or(0);
    validate_axis_index("kain_geometry_face", index, face_count)?;
    let start = usize::try_from(index)
        .map_err(|_| KainError::runtime(format!("kain_geometry_face: invalid index {index}")))?
        * usize::try_from(geometry.face_size).map_err(|_| {
            KainError::runtime(format!(
                "kain_geometry_face: invalid face size {}",
                geometry.face_size
            ))
        })?;
    native_vector_from_buffer(indices, start, geometry.face_size)
}

fn native_geometry_set_face(
    geometry: &KainNativeGeometry,
    index: i64,
    value: &Value,
) -> KainResult<()> {
    let Some(indices) = geometry.indices.as_ref() else {
        return Err(KainError::runtime(
            "kain_geometry_set_face: geometry has no index buffer",
        ));
    };
    let face_count = geometry.index_shape.first().copied().unwrap_or(0);
    validate_axis_index("kain_geometry_set_face", index, face_count)?;
    validate_vector_value("kain_geometry_set_face", value, geometry.face_size)?;
    let start = usize::try_from(index).map_err(|_| {
        KainError::runtime(format!("kain_geometry_set_face: invalid index {index}"))
    })? * usize::try_from(geometry.face_size).map_err(|_| {
        KainError::runtime(format!(
            "kain_geometry_set_face: invalid face size {}",
            geometry.face_size
        ))
    })?;
    native_vector_write(indices, start, geometry.face_size, value)
}

fn native_vector_from_buffer(
    buffer: &NativeScalarBuffer,
    start: usize,
    count: i64,
) -> KainResult<Value> {
    let count = usize::try_from(count)
        .map_err(|_| KainError::runtime(format!("Invalid vector length {count}")))?;
    let mut values = Vec::with_capacity(count);
    for offset in 0..count {
        values.push(buffer.get_value(start + offset)?);
    }
    Ok(Value::Array(Arc::new(RwLock::new(values))))
}

fn native_vector_write(
    buffer: &NativeScalarBuffer,
    start: usize,
    count: i64,
    value: &Value,
) -> KainResult<()> {
    let values = collect_vector_values(value, count)?;
    for (offset, value) in values.iter().enumerate() {
        buffer.set_value(start + offset, value)?;
    }
    Ok(())
}

fn export_native_image_pyobject(
    py: Python<'_>,
    image: &KainNativeImage,
    backend: &str,
) -> KainResult<PyObject> {
    if image.data.is_shared() {
        if let Some(source) = &image.source {
            if matches!(backend, "numpy" | "torch")
                && native_source_matches_backend(source.as_ref(py), backend)
            {
                return Ok(source.clone_ref(py));
            }
        }
    }
    if let Some(owner) = image.data.shared_owner(py) {
        if image.data.shared_backend() == Some(backend) {
            return Ok(owner);
        }
    }
    match backend {
        "numpy" => native_numpy_array(py, &image.data, &image.dtype, &image.shape),
        "torch" => {
            let array = native_numpy_array(py, &image.data, &image.dtype, &image.shape)?;
            let torch = py
                .import("torch")
                .map_err(|err| KainError::runtime(format!("kain_image_to_py: torch import error: {err}")))?;
            torch
                .getattr("from_numpy")
                .and_then(|callable| callable.call1((array.as_ref(py),)))
                .map(|value| value.into_py(py))
                .map_err(|err| KainError::runtime(format!("kain_image_to_py: torch export error: {err}")))
        }
        other => Err(KainError::runtime(format!(
            "kain_image_to_py: unsupported backend {other}; expected numpy or torch"
        ))),
    }
}

fn export_native_tensor_pyobject(
    py: Python<'_>,
    tensor: &KainNativeTensor,
    backend: &str,
) -> KainResult<PyObject> {
    if tensor.data.is_shared() {
        if let Some(source) = &tensor.source {
            if matches!(backend, "numpy" | "torch")
                && native_source_matches_backend(source.as_ref(py), backend)
            {
                return Ok(source.clone_ref(py));
            }
        }
    }
    if let Some(owner) = tensor.data.shared_owner(py) {
        if tensor.data.shared_backend() == Some(backend) {
            return Ok(owner);
        }
    }
    match backend {
        "numpy" => native_numpy_array(py, &tensor.data, &tensor.dtype, &tensor.shape),
        "torch" => {
            let array = native_numpy_array(py, &tensor.data, &tensor.dtype, &tensor.shape)?;
            let torch = py
                .import("torch")
                .map_err(|err| KainError::runtime(format!("kain_tensor_to_py: torch import error: {err}")))?;
            torch
                .getattr("from_numpy")
                .and_then(|callable| callable.call1((array.as_ref(py),)))
                .map(|value| value.into_py(py))
                .map_err(|err| KainError::runtime(format!("kain_tensor_to_py: torch export error: {err}")))
        }
        other => Err(KainError::runtime(format!(
            "kain_tensor_to_py: unsupported backend {other}; expected numpy or torch"
        ))),
    }
}

fn export_native_geometry_pyobject(
    py: Python<'_>,
    geometry: &KainNativeGeometry,
    backend: &str,
) -> KainResult<PyObject> {
    if backend == "trimesh" && geometry.vertices.is_shared() {
        if let Some(source) = &geometry.source {
            if python_type_path(source.as_ref(py)).starts_with("trimesh.") {
                return Ok(source.clone_ref(py));
            }
        }
    }
    let vertices = native_numpy_array(py, &geometry.vertices, &geometry.vertex_dtype, &geometry.vertex_shape)?;
    let faces = match (&geometry.indices, geometry.index_dtype.as_deref()) {
        (Some(indices), Some(dtype)) => Some(native_numpy_array(py, indices, dtype, &geometry.index_shape)?),
        _ => None,
    };

    match backend {
        "dict" => {
            let dict = PyDict::new(py);
            dict.set_item("vertices", vertices.as_ref(py))
                .map_err(|err| KainError::runtime(format!("kain_geometry_to_py: dict export error: {err}")))?;
            match &faces {
                Some(value) => dict
                    .set_item("faces", value.as_ref(py))
                    .map_err(|err| KainError::runtime(format!("kain_geometry_to_py: dict export error: {err}")))?,
                None => dict
                    .set_item("faces", py.None())
                    .map_err(|err| KainError::runtime(format!("kain_geometry_to_py: dict export error: {err}")))?,
            }
            Ok(dict.into())
        }
        "tuple" => Ok(PyTuple::new(
            py,
            [
                vertices.clone_ref(py),
                faces
                    .as_ref()
                    .map(|value| value.clone_ref(py))
                    .unwrap_or_else(|| py.None()),
            ],
        )
        .into()),
        "trimesh" => {
            let trimesh = py
                .import("trimesh")
                .map_err(|err| KainError::runtime(format!("kain_geometry_to_py: trimesh import error: {err}")))?;
            if let Some(faces) = faces {
                let kwargs = PyDict::new(py);
                kwargs
                    .set_item("vertices", vertices.as_ref(py))
                    .map_err(|err| KainError::runtime(format!("kain_geometry_to_py: trimesh kwargs error: {err}")))?;
                kwargs
                    .set_item("faces", faces.as_ref(py))
                    .map_err(|err| KainError::runtime(format!("kain_geometry_to_py: trimesh kwargs error: {err}")))?;
                kwargs
                    .set_item("process", false)
                    .map_err(|err| KainError::runtime(format!("kain_geometry_to_py: trimesh kwargs error: {err}")))?;
                trimesh
                    .getattr("Trimesh")
                    .and_then(|callable| callable.call((), Some(kwargs)))
                    .map(|value| value.into_py(py))
                    .map_err(|err| KainError::runtime(format!("kain_geometry_to_py: trimesh export error: {err}")))
            } else {
                let points = trimesh
                    .getattr("points")
                    .map_err(|err| KainError::runtime(format!("kain_geometry_to_py: trimesh points error: {err}")))?;
                points
                    .getattr("PointCloud")
                    .and_then(|callable| callable.call1((vertices.as_ref(py),)))
                    .map(|value| value.into_py(py))
                    .map_err(|err| KainError::runtime(format!("kain_geometry_to_py: point cloud export error: {err}")))
            }
        }
        other => Err(KainError::runtime(format!(
            "kain_geometry_to_py: unsupported backend {other}; expected dict, tuple, or trimesh"
        ))),
    }
}

fn reshape_python_array(
    py: Python<'_>,
    array: &PyAny,
    shape: &[i64],
    context: &str,
) -> KainResult<PyObject> {
    if shape.is_empty() {
        return array
            .call_method1("reshape", (PyTuple::empty(py),))
            .map(|value| value.into_py(py))
            .map_err(|err| KainError::runtime(format!("{context} error: {err}")));
    }
    let shape_tuple = PyTuple::new(py, shape.iter().copied());
    array
        .call_method1("reshape", (shape_tuple,))
        .map(|value| value.into_py(py))
        .map_err(|err| KainError::runtime(format!("{context} error: {err}")))
}

fn native_source_matches_backend(source: &PyAny, backend: &str) -> bool {
    match backend {
        "numpy" => detect_backend(source) == "numpy",
        "torch" => detect_backend(source) == "torch",
        _ => false,
    }
}

fn native_numpy_array(
    py: Python<'_>,
    buffer: &NativeScalarBuffer,
    dtype: &str,
    shape: &[i64],
) -> KainResult<PyObject> {
    if let Some(owner) = buffer.shared_owner(py) {
        if buffer.shared_backend() == Some("numpy") {
            return reshape_python_array(py, owner.as_ref(py), shape, "NumPy shared reshape");
        }
        if buffer.shared_backend() == Some("torch") {
            let numpy_view = owner
                .as_ref(py)
                .call_method0("detach")
                .and_then(|value| value.call_method0("numpy"))
                .map_err(|err| KainError::runtime(format!("PyTorch shared numpy export error: {err}")))?;
            return reshape_python_array(py, numpy_view, shape, "PyTorch shared numpy reshape");
        }
    }
    let numpy = py
        .import("numpy")
        .map_err(|err| KainError::runtime(format!("NumPy import error: {err}")))?;
    let dtype_object = numpy
        .getattr("dtype")
        .and_then(|callable| callable.call1((dtype,)))
        .map_err(|err| KainError::runtime(format!("NumPy dtype error for {dtype}: {err}")))?;
    let bytes = native_scalar_buffer_bytes(buffer, dtype)?;
    let bytearray = PyByteArray::new(py, &bytes);
    let array = numpy
        .getattr("frombuffer")
        .and_then(|callable| callable.call1((bytearray, dtype_object)))
        .map_err(|err| KainError::runtime(format!("NumPy frombuffer export error: {err}")))?;
    reshape_python_array(py, array, shape, "NumPy reshape export")
}

fn native_scalar_buffer_bytes(buffer: &NativeScalarBuffer, dtype: &str) -> KainResult<Vec<u8>> {
    if let NativeScalarBuffer::Shared(shared) = buffer {
        return Python::with_gil(|py| {
            let bytes = export_payload_bytes(py, shared.owner.as_ref(py))?;
            let bytes = bytes.as_ref(py).downcast::<PyBytes>().map_err(|_| {
                KainError::runtime("Expected bytes when exporting shared Python buffer")
            })?;
            Ok(bytes.as_bytes().to_vec())
        });
    }
    match dtype {
        "bool" | "bool_" => match buffer {
            NativeScalarBuffer::Bool(values) => Ok(values
                .read()
                .unwrap()
                .iter()
                .map(|value| if *value { 1 } else { 0 })
                .collect()),
            _ => Err(KainError::runtime(format!(
                "Native buffer storage mismatch: dtype {dtype} requires bool storage"
            ))),
        },
        "uint8" | "ubyte" => encode_native_int_bytes(buffer, dtype, |value| {
            let narrowed = u8::try_from(value).map_err(|_| {
                KainError::runtime(format!("Cannot encode value {value} as {dtype}"))
            })?;
            Ok(vec![narrowed])
        }),
        "int8" | "byte" => encode_native_int_bytes(buffer, dtype, |value| {
            let narrowed = i8::try_from(value).map_err(|_| {
                KainError::runtime(format!("Cannot encode value {value} as {dtype}"))
            })?;
            Ok(vec![narrowed as u8])
        }),
        "uint16" => encode_native_int_bytes(buffer, dtype, |value| {
            let narrowed = u16::try_from(value).map_err(|_| {
                KainError::runtime(format!("Cannot encode value {value} as {dtype}"))
            })?;
            Ok(narrowed.to_le_bytes().to_vec())
        }),
        "int16" => encode_native_int_bytes(buffer, dtype, |value| {
            let narrowed = i16::try_from(value).map_err(|_| {
                KainError::runtime(format!("Cannot encode value {value} as {dtype}"))
            })?;
            Ok(narrowed.to_le_bytes().to_vec())
        }),
        "uint32" => encode_native_int_bytes(buffer, dtype, |value| {
            let narrowed = u32::try_from(value).map_err(|_| {
                KainError::runtime(format!("Cannot encode value {value} as {dtype}"))
            })?;
            Ok(narrowed.to_le_bytes().to_vec())
        }),
        "int32" => encode_native_int_bytes(buffer, dtype, |value| {
            let narrowed = i32::try_from(value).map_err(|_| {
                KainError::runtime(format!("Cannot encode value {value} as {dtype}"))
            })?;
            Ok(narrowed.to_le_bytes().to_vec())
        }),
        "uint64" => encode_native_int_bytes(buffer, dtype, |value| {
            let narrowed = u64::try_from(value).map_err(|_| {
                KainError::runtime(format!("Cannot encode value {value} as {dtype}"))
            })?;
            Ok(narrowed.to_le_bytes().to_vec())
        }),
        "int64" => encode_native_int_bytes(buffer, dtype, |value| Ok(value.to_le_bytes().to_vec())),
        "float32" => encode_native_float_bytes(buffer, dtype, |value| {
            Ok((value as f32).to_le_bytes().to_vec())
        }),
        "float64" | "double" => encode_native_float_bytes(buffer, dtype, |value| {
            Ok(value.to_le_bytes().to_vec())
        }),
        other => Err(KainError::runtime(format!(
            "Unsupported dtype for native Python export: {other}"
        ))),
    }
}

fn encode_native_int_bytes<F>(
    buffer: &NativeScalarBuffer,
    dtype: &str,
    encode: F,
) -> KainResult<Vec<u8>>
where
    F: Fn(i64) -> KainResult<Vec<u8>>,
{
    let NativeScalarBuffer::Int(values) = buffer else {
        return Err(KainError::runtime(format!(
            "Native buffer storage mismatch: dtype {dtype} requires int storage"
        )));
    };
    let values = values.read().unwrap();
    let mut bytes = Vec::new();
    for value in values.iter().copied() {
        bytes.extend(encode(value)?);
    }
    Ok(bytes)
}

fn encode_native_float_bytes<F>(
    buffer: &NativeScalarBuffer,
    dtype: &str,
    encode: F,
) -> KainResult<Vec<u8>>
where
    F: Fn(f64) -> KainResult<Vec<u8>>,
{
    let NativeScalarBuffer::Float(values) = buffer else {
        return Err(KainError::runtime(format!(
            "Native buffer storage mismatch: dtype {dtype} requires float storage"
        )));
    };
    let values = values.read().unwrap();
    let mut bytes = Vec::new();
    for value in values.iter().copied() {
        bytes.extend(encode(value)?);
    }
    Ok(bytes)
}

fn native_image_channel_indices(
    image: &KainNativeImage,
    batch: i64,
    x: i64,
    y: i64,
) -> KainResult<Vec<usize>> {
    if batch < 0 || batch >= image.batch {
        return Err(KainError::runtime(format!(
            "image batch index {batch} is outside 0..{}",
            image.batch.saturating_sub(1)
        )));
    }
    if x < 0 || x >= image.width {
        return Err(KainError::runtime(format!(
            "image x index {x} is outside 0..{}",
            image.width.saturating_sub(1)
        )));
    }
    if y < 0 || y >= image.height {
        return Err(KainError::runtime(format!(
            "image y index {y} is outside 0..{}",
            image.height.saturating_sub(1)
        )));
    }

    let width = usize::try_from(image.width)
        .map_err(|_| KainError::runtime(format!("Invalid image width {}", image.width)))?;
    let height = usize::try_from(image.height)
        .map_err(|_| KainError::runtime(format!("Invalid image height {}", image.height)))?;
    let channels = usize::try_from(image.channels)
        .map_err(|_| KainError::runtime(format!("Invalid image channels {}", image.channels)))?;
    let batch = usize::try_from(batch)
        .map_err(|_| KainError::runtime(format!("Invalid image batch {batch}")))?;
    let x = usize::try_from(x).map_err(|_| KainError::runtime(format!("Invalid image x {x}")))?;
    let y = usize::try_from(y).map_err(|_| KainError::runtime(format!("Invalid image y {y}")))?;

    let indices = match image.layout.as_str() {
        "HW" => vec![y * width + x],
        "HWC" => {
            let base = (y * width + x) * channels;
            (0..channels).map(|channel| base + channel).collect()
        }
        "CHW" => {
            let plane = height * width;
            (0..channels)
                .map(|channel| channel * plane + y * width + x)
                .collect()
        }
        "NHWC" => {
            let batch_stride = height * width * channels;
            let base = batch * batch_stride + (y * width + x) * channels;
            (0..channels).map(|channel| base + channel).collect()
        }
        "NCHW" => {
            let batch_stride = channels * height * width;
            let plane = height * width;
            let batch_base = batch * batch_stride;
            (0..channels)
                .map(|channel| batch_base + channel * plane + y * width + x)
                .collect()
        }
        other => {
            return Err(KainError::runtime(format!(
                "Unsupported native image layout {other}"
            )))
        }
    };
    Ok(indices)
}

fn flatten_row_major_index(fn_name: &str, shape: &[i64], indices: &[i64]) -> KainResult<usize> {
    if shape.len() != indices.len() {
        return Err(KainError::runtime(format!(
            "{fn_name}: expected {} indices, got {}",
            shape.len(),
            indices.len()
        )));
    }

    let mut index = 0usize;
    let mut stride = 1usize;
    for (dimension, coord) in shape.iter().rev().zip(indices.iter().rev()) {
        if *coord < 0 || *coord >= *dimension {
            return Err(KainError::runtime(format!(
                "{fn_name}: index {coord} is outside 0..{}",
                dimension.saturating_sub(1)
            )));
        }
        let coord = usize::try_from(*coord)
            .map_err(|_| KainError::runtime(format!("{fn_name}: invalid index {coord}")))?;
        index = index.saturating_add(coord.saturating_mul(stride));
        stride = stride
            .saturating_mul(usize::try_from(*dimension).map_err(|_| {
                KainError::runtime(format!("{fn_name}: invalid shape {:?}", shape))
            })?);
    }
    Ok(index)
}

fn checked_element_count(shape: &[i64]) -> KainResult<usize> {
    if shape.is_empty() {
        return Ok(1);
    }

    let mut count = 1usize;
    for dim in shape {
        if *dim < 0 {
            return Err(KainError::runtime(format!(
                "Invalid negative dimension in shape {:?}",
                shape
            )));
        }
        count = count.saturating_mul(usize::try_from(*dim).map_err(|_| {
            KainError::runtime(format!("Invalid dimension {dim} in shape {:?}", shape))
        })?);
    }
    Ok(count)
}

fn collect_vector_values(value: &Value, expected_len: i64) -> KainResult<Vec<Value>> {
    match value {
        Value::Array(values) => {
            let values = values.read().unwrap().clone();
            if values.len() as i64 != expected_len {
                return Err(KainError::runtime(format!(
                    "Expected vector length {expected_len}, got {}",
                    values.len()
                )));
            }
            Ok(values)
        }
        Value::Tuple(values) => {
            if values.len() as i64 != expected_len {
                return Err(KainError::runtime(format!(
                    "Expected vector length {expected_len}, got {}",
                    values.len()
                )));
            }
            Ok(values.clone())
        }
        other if expected_len == 1 => Ok(vec![other.clone()]),
        other => Err(KainError::runtime(format!(
            "Expected vector of length {expected_len}, got {other:?}"
        ))),
    }
}

fn validate_vector_value(fn_name: &str, value: &Value, expected_len: i64) -> KainResult<()> {
    let _ = collect_vector_values(value, expected_len)
        .map_err(|err| KainError::runtime(format!("{fn_name}: {err}")))?;
    Ok(())
}

fn validate_pixel_value(fn_name: &str, value: &Value, channels: i64) -> KainResult<()> {
    validate_vector_value(fn_name, value, channels)
}

fn python_pixel_value(py: Python<'_>, value: &Value, channels: i64) -> KainResult<PyObject> {
    if channels == 1 {
        return match value {
            Value::Array(values) if values.read().unwrap().len() == 1 => {
                value_to_pyobject(py, &values.read().unwrap()[0])
            }
            Value::Tuple(values) if values.len() == 1 => value_to_pyobject(py, &values[0]),
            _ => value_to_pyobject(py, value),
        };
    }
    value_to_pyobject(py, value)
}

fn decode_native_scalar_buffer(bytes: &PyAny, dtype: &str) -> KainResult<NativeScalarBuffer> {
    let bytes = bytes.downcast::<PyBytes>().map_err(|_| {
        KainError::runtime("Expected Python bytes payload for native buffer decode")
    })?;
    let bytes = bytes.as_bytes();

    let int_buffer = |values: Vec<i64>| NativeScalarBuffer::Int(Arc::new(RwLock::new(values)));
    let float_buffer = |values: Vec<f64>| NativeScalarBuffer::Float(Arc::new(RwLock::new(values)));
    let bool_buffer = |values: Vec<bool>| NativeScalarBuffer::Bool(Arc::new(RwLock::new(values)));

    match dtype {
        "bool" | "bool_" => Ok(bool_buffer(bytes.iter().map(|value| *value != 0).collect())),
        "uint8" | "ubyte" => Ok(int_buffer(
            bytes.iter().map(|value| i64::from(*value)).collect(),
        )),
        "int8" | "byte" => Ok(int_buffer(
            bytes.iter().map(|value| i64::from(*value as i8)).collect(),
        )),
        "uint16" => Ok(int_buffer(
            bytes
                .chunks_exact(2)
                .map(|chunk| i64::from(u16::from_le_bytes([chunk[0], chunk[1]])))
                .collect(),
        )),
        "int16" => Ok(int_buffer(
            bytes
                .chunks_exact(2)
                .map(|chunk| i64::from(i16::from_le_bytes([chunk[0], chunk[1]])))
                .collect(),
        )),
        "uint32" => Ok(int_buffer(
            bytes
                .chunks_exact(4)
                .map(|chunk| {
                    i64::from(u32::from_le_bytes([chunk[0], chunk[1], chunk[2], chunk[3]]))
                })
                .collect(),
        )),
        "int32" => Ok(int_buffer(
            bytes
                .chunks_exact(4)
                .map(|chunk| {
                    i64::from(i32::from_le_bytes([chunk[0], chunk[1], chunk[2], chunk[3]]))
                })
                .collect(),
        )),
        "uint64" => {
            let mut values = Vec::with_capacity(bytes.len() / 8);
            for chunk in bytes.chunks_exact(8) {
                let raw = u64::from_le_bytes([
                    chunk[0], chunk[1], chunk[2], chunk[3], chunk[4], chunk[5], chunk[6], chunk[7],
                ]);
                let value = i64::try_from(raw).map_err(|_| {
                    KainError::runtime(format!(
                        "Cannot materialize uint64 value {raw} into Kain Int"
                    ))
                })?;
                values.push(value);
            }
            Ok(int_buffer(values))
        }
        "int64" => Ok(int_buffer(
            bytes
                .chunks_exact(8)
                .map(|chunk| {
                    i64::from_le_bytes([
                        chunk[0], chunk[1], chunk[2], chunk[3], chunk[4], chunk[5], chunk[6],
                        chunk[7],
                    ])
                })
                .collect(),
        )),
        "float32" => Ok(float_buffer(
            bytes
                .chunks_exact(4)
                .map(|chunk| f32::from_le_bytes([chunk[0], chunk[1], chunk[2], chunk[3]]) as f64)
                .collect(),
        )),
        "float64" | "double" => Ok(float_buffer(
            bytes
                .chunks_exact(8)
                .map(|chunk| {
                    f64::from_le_bytes([
                        chunk[0], chunk[1], chunk[2], chunk[3], chunk[4], chunk[5], chunk[6],
                        chunk[7],
                    ])
                })
                .collect(),
        )),
        other => Err(KainError::runtime(format!(
            "Unsupported dtype for native Kain buffer materialization: {other}"
        ))),
    }
}

fn extract_native_image(value: &Value) -> KainResult<Arc<KainNativeImage>> {
    value
        .downcast_host_object::<KainNativeImage>()
        .ok_or_else(|| {
            KainError::runtime(format!(
                "Expected Kain native image, got {}",
                value.host_object_label().unwrap_or("value")
            ))
        })
}

fn extract_native_tensor(value: &Value) -> KainResult<Arc<KainNativeTensor>> {
    value
        .downcast_host_object::<KainNativeTensor>()
        .ok_or_else(|| {
            KainError::runtime(format!(
                "Expected Kain native tensor, got {}",
                value.host_object_label().unwrap_or("value")
            ))
        })
}

fn extract_native_geometry(value: &Value) -> KainResult<Arc<KainNativeGeometry>> {
    value
        .downcast_host_object::<KainNativeGeometry>()
        .ok_or_else(|| {
            KainError::runtime(format!(
                "Expected Kain native geometry, got {}",
                value.host_object_label().unwrap_or("value")
            ))
        })
}

fn detect_backend(target: &PyAny) -> String {
    let kind = python_type_path(target);
    if kind.starts_with("numpy.") {
        "numpy".to_string()
    } else if kind.starts_with("torch.") {
        "torch".to_string()
    } else if is_numpy_array_like(target) {
        "numpy".to_string()
    } else {
        "python".to_string()
    }
}

fn is_numpy_array_like(target: &PyAny) -> bool {
    target.hasattr("__array_interface__").unwrap_or(false)
        || target.hasattr("__array__").unwrap_or(false)
}

fn is_torch_tensor(target: &PyAny) -> bool {
    let type_name = python_type_path(target);
    type_name == "torch.Tensor"
}

fn torch_dtype_name(target: &PyAny) -> KainResult<String> {
    let dtype = target
        .getattr("dtype")
        .map_err(|err| KainError::runtime(format!("PyTorch dtype error: {err}")))?;
    let name = dtype
        .str()
        .and_then(|value| value.extract::<String>())
        .map_err(|err| KainError::runtime(format!("PyTorch dtype string error: {err}")))?;
    Ok(name.trim_start_matches("torch.").to_string())
}

fn torch_item_size(target: &PyAny) -> KainResult<i64> {
    target
        .call_method0("element_size")
        .and_then(|value| value.extract::<i64>())
        .map_err(|err| KainError::runtime(format!("PyTorch element_size error: {err}")))
}

fn torch_nbytes(target: &PyAny) -> KainResult<i64> {
    let element_size = torch_item_size(target)?;
    let numel = target
        .call_method0("numel")
        .and_then(|value| value.extract::<i64>())
        .map_err(|err| KainError::runtime(format!("PyTorch numel error: {err}")))?;
    Ok(element_size.saturating_mul(numel))
}

fn torch_is_contiguous(target: &PyAny) -> KainResult<bool> {
    target
        .call_method0("is_contiguous")
        .and_then(|value| value.extract::<bool>())
        .map_err(|err| KainError::runtime(format!("PyTorch is_contiguous error: {err}")))
}

fn torch_stride_values(target: &PyAny) -> KainResult<Vec<i64>> {
    target
        .call_method0("stride")
        .map_err(|err| KainError::runtime(format!("PyTorch stride error: {err}")))
        .and_then(|value| py_index_sequence_values(value, "stride"))
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
    if let Some(object) = value.downcast_host_object::<PythonObjectRef>() {
        return Ok(object.object.clone_ref(py));
    }
    if let Some(view) = value.downcast_host_object::<PythonImageView>() {
        return Ok(view.object.clone_ref(py));
    }
    if let Some(view) = value.downcast_host_object::<PythonTensorView>() {
        return Ok(view.object.clone_ref(py));
    }
    if let Some(image) = value.downcast_host_object::<KainNativeImage>() {
        return export_native_image_pyobject(py, image.as_ref(), "numpy");
    }
    if let Some(tensor) = value.downcast_host_object::<KainNativeTensor>() {
        return export_native_tensor_pyobject(py, tensor.as_ref(), "numpy");
    }
    if let Some(geometry) = value.downcast_host_object::<KainNativeGeometry>() {
        return export_native_geometry_pyobject(py, geometry.as_ref(), "dict");
    }

    Err(KainError::runtime(format!(
        "Expected Python object handle, got {}",
        value.host_object_label().unwrap_or("host object")
    )))
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
    if let Some(value) = try_array_like_to_value(obj)? {
        return Ok(value);
    }

    wrap_python_object(obj)
}

fn py_any_to_value(obj: &PyAny) -> KainResult<Value> {
    if is_torch_tensor(obj) {
        let ndim = py_int_attr_value(obj, "ndim")?;
        if ndim == 0 {
            let scalar = obj
                .call_method0("item")
                .map_err(|err| KainError::runtime(format!("PyTorch scalar item error: {err}")))?;
            return py_to_value(scalar);
        }
    }
    py_to_value(obj)
}

fn try_array_like_to_value(obj: &PyAny) -> KainResult<Option<Value>> {
    if is_torch_tensor(obj) {
        return Ok(None);
    }

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

    let is_numpy_array = module_name.starts_with("numpy") && type_name == "ndarray";
    let is_array_like = obj.hasattr("tolist").unwrap_or(false)
        && (obj.hasattr("shape").unwrap_or(false)
            || obj.hasattr("__array_interface__").unwrap_or(false));

    if !is_numpy_array && !is_array_like {
        return Ok(None);
    }

    let list_value = obj
        .call_method0("tolist")
        .map_err(|err| KainError::runtime(format!("Python array conversion error: {err}")))?;
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
    use std::sync::{Mutex, OnceLock};

    fn python_test_lock() -> &'static Mutex<()> {
        static LOCK: OnceLock<Mutex<()>> = OnceLock::new();
        LOCK.get_or_init(|| Mutex::new(()))
    }

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
        assert!(stdlib.functions.contains_key("py_tensor_info"));
        assert!(stdlib.functions.contains_key("py_tensor_bytes"));
        assert!(stdlib.functions.contains_key("py_image_info"));
        assert!(stdlib.functions.contains_key("py_image_view"));
        assert!(stdlib.functions.contains_key("py_image_pixel"));
        assert!(stdlib.functions.contains_key("py_image_set_pixel"));
        assert!(stdlib.functions.contains_key("py_geometry_info"));
        assert!(stdlib.functions.contains_key("py_geometry_view"));
        assert!(stdlib.functions.contains_key("py_geometry_vertex"));
        assert!(stdlib.functions.contains_key("py_geometry_face"));
        assert!(stdlib.functions.contains_key("py_geometry_set_vertex"));
        assert!(stdlib.functions.contains_key("py_geometry_set_face"));
        assert!(stdlib.functions.contains_key("py_tensor_view"));
        assert!(stdlib.functions.contains_key("py_tensor_get"));
        assert!(stdlib.functions.contains_key("py_tensor_set"));
        assert!(stdlib.functions.contains_key("kain_image_from_py"));
        assert!(stdlib.functions.contains_key("kain_image_info"));
        assert!(stdlib.functions.contains_key("kain_image_pixel"));
        assert!(stdlib.functions.contains_key("kain_image_set_pixel"));
        assert!(stdlib.functions.contains_key("kain_image_to_py"));
        assert!(stdlib.functions.contains_key("kain_tensor_from_py"));
        assert!(stdlib.functions.contains_key("kain_tensor_info"));
        assert!(stdlib.functions.contains_key("kain_tensor_get"));
        assert!(stdlib.functions.contains_key("kain_tensor_set"));
        assert!(stdlib.functions.contains_key("kain_tensor_to_py"));
        assert!(stdlib.functions.contains_key("kain_geometry_from_py"));
        assert!(stdlib.functions.contains_key("kain_geometry_info"));
        assert!(stdlib.functions.contains_key("kain_geometry_vertex"));
        assert!(stdlib.functions.contains_key("kain_geometry_set_vertex"));
        assert!(stdlib.functions.contains_key("kain_geometry_face"));
        assert!(stdlib.functions.contains_key("kain_geometry_set_face"));
        assert!(stdlib.functions.contains_key("kain_geometry_to_py"));
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

    #[test]
    fn python_bridge_infers_image_metadata_when_available() {
        if !numpy_available() {
            eprintln!("skipping image metadata test because numpy is not installed");
            return;
        }

        let result = interpret_source(
            r#"
fn main():
    py_exec("import numpy as np\ndef make_image():\n    return np.zeros((12, 20, 4), dtype=np.uint8)")
    let image = py_call_raw("make_image", [])
    let info = py_image_info(image)
    assert(info.width == 20, "expected width 20")
    assert(info.height == 12, "expected height 12")
    assert(info.channels == 4, "expected rgba")
    assert(info.layout == "HWC", "expected channel-last image")
    return info.pixel_count
"#,
        );

        match result {
            Value::Int(value) => assert_eq!(value, 240),
            other => panic!("expected image bridge to return Int(240), got {other:?}"),
        }
    }

    #[test]
    fn python_bridge_reads_pixels_from_image_views_when_available() {
        if !numpy_available() {
            eprintln!("skipping image view test because numpy is not installed");
            return;
        }

        let result = interpret_source(
            r#"
fn main():
    py_exec("import numpy as np\ndef make_image():\n    image = np.zeros((3, 4, 4), dtype=np.uint8)\n    image[1, 2] = np.array([10, 20, 30, 255], dtype=np.uint8)\n    return image")
    let image = py_call_raw("make_image", [])
    let view = py_image_view(image)
    let pixel = py_image_pixel(view, 2, 1)
    assert(len(pixel) == 4, "expected rgba pixel")
    assert(pixel[0] == 10, "expected red channel")
    assert(pixel[1] == 20, "expected green channel")
    assert(pixel[2] == 30, "expected blue channel")
    return pixel[3]
"#,
        );

        match result {
            Value::Int(value) => assert_eq!(value, 255),
            other => panic!("expected image view to return alpha channel, got {other:?}"),
        }
    }

    #[test]
    fn python_bridge_mutates_live_image_views_when_available() {
        if !numpy_available() {
            eprintln!("skipping live image mutation test because numpy is not installed");
            return;
        }

        let result = interpret_source(
            r#"
fn main():
    py_exec("import numpy as np\ndef make_image():\n    return np.zeros((3, 4, 4), dtype=np.uint8)")
    let image = py_call_raw("make_image", [])
    let view = py_image_view(image)
    py_image_set_pixel(view, 2, 1, [12, 34, 56, 255])
    let pixel = py_image_pixel(view, 2, 1)
    return pixel[2]
"#,
        );

        match result {
            Value::Int(value) => assert_eq!(value, 56),
            other => panic!("expected live image mutation to return Int(56), got {other:?}"),
        }
    }

    #[test]
    fn python_bridge_infers_geometry_metadata_from_trimesh_when_available() {
        if !trimesh_available() {
            eprintln!("skipping geometry metadata test because trimesh is not installed");
            return;
        }

        let result = interpret_source(
            r#"
fn main():
    py_exec("import trimesh\ndef make_mesh():\n    return trimesh.creation.icosphere(subdivisions=1, radius=1.0)")
    let mesh = py_call_raw("make_mesh", [])
    let info = py_geometry_info(mesh)
    assert(info.primitive == "mesh", "expected mesh primitive")
    assert(info.components == 3, "expected xyz vertices")
    assert(info.face_size == 3, "expected triangle faces")
    return info.vertex_count
"#,
        );

        match result {
            Value::Int(value) => assert!(value > 0),
            other => panic!("expected geometry bridge to return vertex count, got {other:?}"),
        }
    }

    #[test]
    fn python_bridge_reads_geometry_samples_from_views_when_available() {
        if !trimesh_available() {
            eprintln!("skipping geometry view test because trimesh is not installed");
            return;
        }

        let result = interpret_source(
            r#"
fn main():
    py_exec("import trimesh\ndef make_mesh():\n    return trimesh.creation.box(extents=(2.0, 4.0, 6.0))")
    let mesh = py_call_raw("make_mesh", [])
    let view = py_geometry_view(mesh)
    let point = py_geometry_vertex(view, 0)
    let face = py_geometry_face(view, 0)
    assert(len(point) == 3, "expected xyz vertex")
    assert(len(face) == 3, "expected triangle face")
    return len(point) + len(face)
"#,
        );

        match result {
            Value::Int(value) => assert_eq!(value, 6),
            other => panic!("expected geometry view sample size, got {other:?}"),
        }
    }

    #[test]
    fn python_bridge_reads_torch_tensor_metadata_when_available() {
        if !torch_available() {
            eprintln!("skipping torch metadata test because torch is not installed");
            return;
        }

        let result = interpret_source(
            r#"
fn main():
    py_exec("import torch\ndef make_tensor():\n    return torch.arange(0, 96, dtype=torch.float32).reshape(2, 3, 4, 4)")
    let tensor = py_call_raw("make_tensor", [])
    let info = py_tensor_info(tensor)
    assert(info.backend == "torch", "expected torch backend")
    assert(info.dtype == "float32", "expected float32 tensor")
    assert(info.shape[0] == 2, "expected batch 2")
    assert(info.shape[1] == 3, "expected channels 3")
    assert(info.nbytes == 96 * 4, "expected float32 byte size")
    let bytes = py_tensor_bytes(tensor)
    return len(bytes)
"#,
        );

        match result {
            Value::Int(value) => assert_eq!(value, 384),
            other => panic!("expected torch bridge to return byte count, got {other:?}"),
        }
    }

    #[test]
    fn python_bridge_reads_tensor_scalars_from_views_when_available() {
        if !torch_available() {
            eprintln!("skipping tensor view test because torch is not installed");
            return;
        }

        let result = interpret_source(
            r#"
fn main():
    py_exec("import torch\ndef make_tensor():\n    return torch.arange(0, 24, dtype=torch.float32).reshape(2, 3, 4)")
    let tensor = py_call_raw("make_tensor", [])
    let view = py_tensor_view(tensor)
    return py_tensor_get(view, [1, 2, 3])
"#,
        );

        match result {
            Value::Float(value) => assert_eq!(value, 23.0),
            other => panic!("expected tensor view to return Float(23.0), got {other:?}"),
        }
    }

    #[test]
    fn python_bridge_materializes_native_image_buffers_when_available() {
        if !numpy_available() {
            eprintln!("skipping native image test because numpy is not installed");
            return;
        }

        let result = interpret_source(
            r#"
fn main():
    py_exec("import numpy as np\ndef make_image():\n    image = np.zeros((4, 5, 4), dtype=np.uint8)\n    image[2, 3] = np.array([1, 2, 3, 255], dtype=np.uint8)\n    return image")
    let native = kain_image_from_py(py_call_raw("make_image", []))
    let info = kain_image_info(native)
    assert(info.width == 5, "expected width 5")
    assert(info.height == 4, "expected height 4")
    let before = kain_image_pixel(native, 3, 2)
    assert(before[2] == 3, "expected blue channel before write")
    kain_image_set_pixel(native, 3, 2, [9, 8, 7, 255])
    return kain_image_pixel(native, 3, 2)[0]
"#,
        );

        match result {
            Value::Int(value) => assert_eq!(value, 9),
            other => panic!("expected native image mutation to return Int(9), got {other:?}"),
        }
    }

    #[test]
    fn python_bridge_syncs_native_image_mutations_into_original_numpy_when_available() {
        if !numpy_available() {
            eprintln!("skipping native image sync test because numpy is not installed");
            return;
        }

        let result = interpret_source(
            r#"
fn main():
    py_exec("import numpy as np\ndef make_image():\n    return np.zeros((4, 5, 4), dtype=np.uint8)")
    let image = py_call_raw("make_image", [])
    let native = kain_image_from_py(image)
    let info = kain_image_info(native)
    assert(info.zero_copy == true, "expected zero-copy native image backing")
    kain_image_set_pixel(native, 3, 2, [5, 6, 7, 255])
    return py_image_pixel(py_image_view(image), 3, 2)[2]
"#,
        );

        match result {
            Value::Int(value) => assert_eq!(value, 7),
            other => panic!("expected native image sync to return Int(7), got {other:?}"),
        }
    }

    #[test]
    fn python_bridge_exports_native_images_back_to_numpy_when_available() {
        if !numpy_available() {
            eprintln!("skipping native image export test because numpy is not installed");
            return;
        }

        let result = interpret_source(
            r#"
fn main():
    py_exec("import numpy as np\ndef make_image():\n    return np.zeros((4, 5, 4), dtype=np.uint8)")
    let native = kain_image_from_py(py_call_raw("make_image", []))
    kain_image_set_pixel(native, 2, 1, [77, 88, 99, 255])
    let exported = kain_image_to_py(native)
    let view = py_image_view(exported)
    return py_image_pixel(view, 2, 1)[2]
"#,
        );

        match result {
            Value::Int(value) => assert_eq!(value, 99),
            other => panic!("expected native image export to return Int(99), got {other:?}"),
        }
    }

    #[test]
    fn python_bridge_materializes_native_tensor_buffers_when_available() {
        if !numpy_available() {
            eprintln!("skipping native tensor test because numpy is not installed");
            return;
        }

        let result = interpret_source(
            r#"
fn main():
    py_exec("import numpy as np\ndef make_tensor():\n    return np.arange(0, 12, dtype=np.float32).reshape(3, 4)")
    let native = kain_tensor_from_py(py_call_raw("make_tensor", []))
    let info = kain_tensor_info(native)
    assert(info.shape[0] == 3, "expected first dim 3")
    assert(info.shape[1] == 4, "expected second dim 4")
    assert(kain_tensor_get(native, [2, 3]) == 11.0, "expected last element 11")
    kain_tensor_set(native, [1, 2], 42.5)
    return kain_tensor_get(native, [1, 2])
"#,
        );

        match result {
            Value::Float(value) => assert_eq!(value, 42.5),
            other => panic!("expected native tensor mutation to return Float(42.5), got {other:?}"),
        }
    }

    #[test]
    fn python_bridge_syncs_native_tensor_mutations_into_original_torch_when_available() {
        if !torch_available() {
            eprintln!("skipping native tensor sync test because torch is not installed");
            return;
        }

        let result = interpret_source(
            r#"
fn main():
    py_exec("import torch\ndef make_tensor():\n    return torch.arange(0, 12, dtype=torch.float32).reshape(3, 4)")
    let tensor = py_call_raw("make_tensor", [])
    let native = kain_tensor_from_py(tensor)
    let info = kain_tensor_info(native)
    assert(info.zero_copy == true, "expected zero-copy native tensor backing")
    kain_tensor_set(native, [1, 2], 42.5)
    return py_tensor_get(py_tensor_view(tensor), [1, 2])
"#,
        );

        match result {
            Value::Float(value) => assert_eq!(value, 42.5),
            other => panic!("expected native tensor sync to return Float(42.5), got {other:?}"),
        }
    }

    #[test]
    fn python_bridge_exports_native_tensors_back_to_torch_when_available() {
        if !numpy_available() || !torch_available() {
            eprintln!("skipping native tensor torch export test because numpy or torch is not installed");
            return;
        }

        let result = interpret_source(
            r#"
fn main():
    py_exec("import numpy as np\ndef make_tensor():\n    return np.arange(0, 12, dtype=np.float32).reshape(3, 4)")
    let native = kain_tensor_from_py(py_call_raw("make_tensor", []))
    kain_tensor_set(native, [1, 2], 42.5)
    let exported = kain_tensor_to_py(native, "torch")
    let info = py_tensor_info(exported)
    assert(info.backend == "torch", "expected torch tensor export")
    let view = py_tensor_view(exported)
    return py_tensor_get(view, [1, 2])
"#,
        );

        match result {
            Value::Float(value) => assert_eq!(value, 42.5),
            other => panic!("expected native tensor export to return Float(42.5), got {other:?}"),
        }
    }

    #[test]
    fn python_bridge_materializes_native_geometry_buffers_when_available() {
        if !numpy_available() {
            eprintln!("skipping native geometry test because numpy is not installed");
            return;
        }

        let result = interpret_source(
            r#"
fn main():
    py_exec("import numpy as np\ndef make_vertices():\n    return np.array([[0.0, 0.0, 0.0], [1.0, 0.0, 0.0], [0.0, 1.0, 0.0]], dtype=np.float32)\ndef make_faces():\n    return np.array([[0, 1, 2]], dtype=np.int32)")
    let geometry = kain_geometry_from_py(py_call_raw("make_vertices", []), py_call_raw("make_faces", []))
    let info = kain_geometry_info(geometry)
    assert(info.vertex_count == 3, "expected 3 vertices")
    assert(info.face_count == 1, "expected 1 face")
    assert(kain_geometry_face(geometry, 0)[2] == 2, "expected triangle face")
    kain_geometry_set_vertex(geometry, 1, [2.5, 3.5, 4.5])
    return kain_geometry_vertex(geometry, 1)[1]
"#,
        );

        match result {
            Value::Float(value) => assert_eq!(value, 3.5),
            other => {
                panic!("expected native geometry mutation to return Float(3.5), got {other:?}")
            }
        }
    }

    #[test]
    fn python_bridge_syncs_native_geometry_mutations_into_original_trimesh_when_available() {
        if !trimesh_available() {
            eprintln!("skipping native geometry sync test because trimesh is not installed");
            return;
        }

        let result = interpret_source(
            r#"
fn main():
    py_exec("import trimesh\ndef make_mesh():\n    return trimesh.creation.icosphere(subdivisions=1, radius=1.0)")
    let mesh = py_call_raw("make_mesh", [])
    let native = kain_geometry_from_py(mesh)
    let info = kain_geometry_info(native)
    assert(info.shared_vertices == true, "expected shared trimesh vertex backing")
    kain_geometry_set_vertex(native, 0, [0.25, 1.25, 0.75])
    return py_geometry_vertex(py_geometry_view(mesh), 0)[1]
"#,
        );

        match result {
            Value::Float(value) => assert_eq!(value, 1.25),
            other => panic!("expected native geometry sync to return Float(1.25), got {other:?}"),
        }
    }

    #[test]
    fn python_bridge_exports_native_geometry_back_to_trimesh_when_available() {
        if !numpy_available() || !trimesh_available() {
            eprintln!("skipping native geometry export test because numpy or trimesh is not installed");
            return;
        }

        let result = interpret_source(
            r#"
fn main():
    py_exec("import numpy as np\ndef make_vertices():\n    return np.array([[0.0, 0.0, 0.0], [1.0, 0.0, 0.0], [0.0, 1.0, 0.0]], dtype=np.float32)\ndef make_faces():\n    return np.array([[0, 1, 2]], dtype=np.int32)")
    let geometry = kain_geometry_from_py(py_call_raw("make_vertices", []), py_call_raw("make_faces", []))
    kain_geometry_set_vertex(geometry, 1, [2.5, 3.5, 4.5])
    let exported = kain_geometry_to_py(geometry, "trimesh")
    let view = py_geometry_view(exported)
    return py_geometry_vertex(view, 1)[1]
"#,
        );

        match result {
            Value::Float(value) => assert_eq!(value, 3.5),
            other => panic!("expected native geometry export to return Float(3.5), got {other:?}"),
        }
    }

    #[test]
    fn python_bridge_passes_native_tensors_into_python_calls_by_default() {
        if !numpy_available() {
            eprintln!("skipping native tensor python-call bridge test because numpy is not installed");
            return;
        }

        let result = interpret_source(
            r#"
fn main():
    py_exec("import numpy as np\ndef tail_value(values):\n    return np.asarray(values)[-1]\ndef make_native():\n    return np.linspace(-1.0, 1.0, 5)")
    let native = kain_tensor_from_py(py_call_raw("make_native", []))
    kain_tensor_set(native, [4], 9.0)
    return py_call("tail_value", [native])
"#,
        );

        match result {
            Value::Float(value) => assert_eq!(value, 9.0),
            other => panic!("expected native tensor python-call bridge to return Float(9.0), got {other:?}"),
        }
    }

    fn interpret_source(source: &str) -> Value {
        let _guard = python_test_lock().lock().unwrap();
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
        let _guard = python_test_lock().lock().unwrap();
        Python::with_gil(|py| py.import("numpy").is_ok())
    }

    fn torch_available() -> bool {
        let _guard = python_test_lock().lock().unwrap();
        Python::with_gil(|py| py.import("torch").is_ok())
    }

    fn trimesh_available() -> bool {
        let _guard = python_test_lock().lock().unwrap();
        Python::with_gil(|py| py.import("trimesh").is_ok())
    }
}
