use kain_core::error::KainError;
use kain_core::runtime::{Env, Value};
use kain_host::{FromKainValue, ToKainValue};
use libloading::{Library, Symbol};
use std::ffi::{c_void, CStr, CString};
use std::sync::{Arc, RwLock};

const SHARED_LIB_PATH: &str = "M:\\Code\\Kain\\smoketest\\3D\\native_sculpt_runtime\\native/native_sculpt_host.dll";

#[derive(Clone)]
struct CAbiOpaqueHandle {
    pointee: String,
    mutable: bool,
    address: usize,
}

struct ByteBufferArg {
    bytes: Vec<u8>,
    writeback: Option<ByteBufferWriteback>,
}

enum ByteBufferWriteback {
    SharedBuffer(Value),
    SharedImage(Value),
}

impl ByteBufferArg {
    fn from_value(env: &mut Env, value: Value, mutable: bool) -> Result<Self, KainError> {
        if value.host_object_label() == Some("kain.shared.image") {
            let snapshot = env.call_named_function(
                "kain_shared_image_bytes",
                vec![value.clone()],
            )?;
            return Ok(Self {
                bytes: bytes_from_value(snapshot)?,
                writeback: if mutable {
                    Some(ByteBufferWriteback::SharedImage(value))
                } else {
                    None
                },
            });
        }
        if value.host_object_label() == Some("kain.shared.buffer") {
            let snapshot = env.call_named_function(
                "kain_shared_buffer_bytes",
                vec![value.clone()],
            )?;
            return Ok(Self {
                bytes: bytes_from_value(snapshot)?,
                writeback: if mutable {
                    Some(ByteBufferWriteback::SharedBuffer(value))
                } else {
                    None
                },
            });
        }
        Ok(Self {
            bytes: <Vec<u8> as FromKainValue>::from_kain_value(value)?,
            writeback: None,
        })
    }

    fn as_ptr(&self) -> *const u8 {
        self.bytes.as_ptr()
    }

    fn as_mut_ptr(&mut self) -> *mut u8 {
        self.bytes.as_mut_ptr()
    }

    fn commit(self, env: &mut Env) -> Result<(), KainError> {
        match self.writeback {
            Some(ByteBufferWriteback::SharedBuffer(buffer)) => {
                env.call_named_function(
                    "kain_shared_buffer_replace_bytes",
                    vec![buffer, bytes_to_value(&self.bytes)],
                )?;
                Ok(())
            }
            Some(ByteBufferWriteback::SharedImage(image)) => {
                env.call_named_function(
                    "kain_shared_image_replace_bytes",
                    vec![image, bytes_to_value(&self.bytes)],
                )?;
                Ok(())
            }
            None => Ok(()),
        }
    }
}

fn bytes_from_value(value: Value) -> Result<Vec<u8>, KainError> {
    <Vec<u8> as FromKainValue>::from_kain_value(value)
}

fn bytes_to_value(bytes: &[u8]) -> Value {
    Value::Array(Arc::new(RwLock::new(
        bytes.iter().map(|value| Value::Int(*value as i64)).collect(),
    )))
}

fn extract_c_handle(
    value: Value,
    expected_pointee: &str,
    allow_null: bool,
) -> Result<*mut c_void, KainError> {
    match value {
        Value::None if allow_null => Ok(std::ptr::null_mut()),
        other => {
            let handle = other
                .downcast_host_object::<CAbiOpaqueHandle>()
                .ok_or_else(|| KainError::runtime("expected a C ABI opaque handle".to_string()))?;
            if expected_pointee != "void" && handle.pointee != expected_pointee {
                return Err(KainError::runtime(format!(
                    "expected C ABI handle for {}, got {}",
                    expected_pointee, handle.pointee
                )));
            }
            Ok(handle.address as *mut c_void)
        }
    }
}

fn __kain_c_bridge_native_sculpt_runtime_average_fps_x100(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 1 {
        return Err(KainError::runtime(format!("native_sculpt_runtime_average_fps_x100 expected 1 argument(s), got {}", args.len())));
    }
    let library = unsafe { Library::new(SHARED_LIB_PATH) }
        .map_err(|err| KainError::runtime(format!("Failed to load C shared library {}: {err}", SHARED_LIB_PATH)))?;
    let mut iter = args.into_iter();
    let runtime_handle = extract_c_handle(iter.next().expect("checked arg count"), "void", true)?;
    let symbol: Symbol<unsafe extern "C" fn(*mut std::ffi::c_void) -> std::os::raw::c_int> = unsafe { library.get(&[110, 97, 116, 105, 118, 101, 95, 115, 99, 117, 108, 112, 116, 95, 114, 117, 110, 116, 105, 109, 101, 95, 97, 118, 101, 114, 97, 103, 101, 95, 102, 112, 115, 95, 120, 49, 48, 48, 0]) }
        .map_err(|err| KainError::runtime(format!("Missing C symbol {} in {}: {err}", "native_sculpt_runtime_average_fps_x100", SHARED_LIB_PATH)))?;
    let result = unsafe { symbol(runtime_handle) };
    Ok(ToKainValue::to_kain_value(result as i64))
}

fn __kain_c_bridge_native_sculpt_runtime_checksum(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 1 {
        return Err(KainError::runtime(format!("native_sculpt_runtime_checksum expected 1 argument(s), got {}", args.len())));
    }
    let library = unsafe { Library::new(SHARED_LIB_PATH) }
        .map_err(|err| KainError::runtime(format!("Failed to load C shared library {}: {err}", SHARED_LIB_PATH)))?;
    let mut iter = args.into_iter();
    let runtime_handle = extract_c_handle(iter.next().expect("checked arg count"), "void", true)?;
    let symbol: Symbol<unsafe extern "C" fn(*mut std::ffi::c_void) -> std::os::raw::c_int> = unsafe { library.get(&[110, 97, 116, 105, 118, 101, 95, 115, 99, 117, 108, 112, 116, 95, 114, 117, 110, 116, 105, 109, 101, 95, 99, 104, 101, 99, 107, 115, 117, 109, 0]) }
        .map_err(|err| KainError::runtime(format!("Missing C symbol {} in {}: {err}", "native_sculpt_runtime_checksum", SHARED_LIB_PATH)))?;
    let result = unsafe { symbol(runtime_handle) };
    Ok(ToKainValue::to_kain_value(result as i64))
}

fn __kain_c_bridge_native_sculpt_runtime_create(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 7 {
        return Err(KainError::runtime(format!("native_sculpt_runtime_create expected 7 argument(s), got {}", args.len())));
    }
    let library = unsafe { Library::new(SHARED_LIB_PATH) }
        .map_err(|err| KainError::runtime(format!("Failed to load C shared library {}: {err}", SHARED_LIB_PATH)))?;
    let mut iter = args.into_iter();
    let width = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))? as std::os::raw::c_int;
    let height = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))? as std::os::raw::c_int;
    let radius = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))? as std::os::raw::c_int;
    let intensity = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))? as std::os::raw::c_int;
    let hardness = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))? as std::os::raw::c_int;
    let target_polys = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))? as std::os::raw::c_int;
    let __title_owned = <String as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let __title_cstring = CString::new(__title_owned).map_err(|_| KainError::runtime("C string argument contained interior NUL".to_string()))?;
    let title = __title_cstring.as_ptr();
    let symbol: Symbol<unsafe extern "C" fn(std::os::raw::c_int, std::os::raw::c_int, std::os::raw::c_int, std::os::raw::c_int, std::os::raw::c_int, std::os::raw::c_int, *const std::os::raw::c_char) -> *mut std::ffi::c_void> = unsafe { library.get(&[110, 97, 116, 105, 118, 101, 95, 115, 99, 117, 108, 112, 116, 95, 114, 117, 110, 116, 105, 109, 101, 95, 99, 114, 101, 97, 116, 101, 0]) }
        .map_err(|err| KainError::runtime(format!("Missing C symbol {} in {}: {err}", "native_sculpt_runtime_create", SHARED_LIB_PATH)))?;
    let result = unsafe { symbol(width, height, radius, intensity, hardness, target_polys, title) };
    if result.is_null() { return Ok(Value::None); }
    Ok(Value::host_object("kain.c.handle", Arc::new(CAbiOpaqueHandle { pointee: "void".to_string(), mutable: true, address: result as usize })))
}

fn __kain_c_bridge_native_sculpt_runtime_destroy(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 1 {
        return Err(KainError::runtime(format!("native_sculpt_runtime_destroy expected 1 argument(s), got {}", args.len())));
    }
    let library = unsafe { Library::new(SHARED_LIB_PATH) }
        .map_err(|err| KainError::runtime(format!("Failed to load C shared library {}: {err}", SHARED_LIB_PATH)))?;
    let mut iter = args.into_iter();
    let runtime_handle = extract_c_handle(iter.next().expect("checked arg count"), "void", true)?;
    let symbol: Symbol<unsafe extern "C" fn(*mut std::ffi::c_void) -> ()> = unsafe { library.get(&[110, 97, 116, 105, 118, 101, 95, 115, 99, 117, 108, 112, 116, 95, 114, 117, 110, 116, 105, 109, 101, 95, 100, 101, 115, 116, 114, 111, 121, 0]) }
        .map_err(|err| KainError::runtime(format!("Missing C symbol {} in {}: {err}", "native_sculpt_runtime_destroy", SHARED_LIB_PATH)))?;
    let result = unsafe { symbol(runtime_handle) };
    Ok(Value::Unit)
}

fn __kain_c_bridge_native_sculpt_runtime_frame_count(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 1 {
        return Err(KainError::runtime(format!("native_sculpt_runtime_frame_count expected 1 argument(s), got {}", args.len())));
    }
    let library = unsafe { Library::new(SHARED_LIB_PATH) }
        .map_err(|err| KainError::runtime(format!("Failed to load C shared library {}: {err}", SHARED_LIB_PATH)))?;
    let mut iter = args.into_iter();
    let runtime_handle = extract_c_handle(iter.next().expect("checked arg count"), "void", true)?;
    let symbol: Symbol<unsafe extern "C" fn(*mut std::ffi::c_void) -> std::os::raw::c_int> = unsafe { library.get(&[110, 97, 116, 105, 118, 101, 95, 115, 99, 117, 108, 112, 116, 95, 114, 117, 110, 116, 105, 109, 101, 95, 102, 114, 97, 109, 101, 95, 99, 111, 117, 110, 116, 0]) }
        .map_err(|err| KainError::runtime(format!("Missing C symbol {} in {}: {err}", "native_sculpt_runtime_frame_count", SHARED_LIB_PATH)))?;
    let result = unsafe { symbol(runtime_handle) };
    Ok(ToKainValue::to_kain_value(result as i64))
}

fn __kain_c_bridge_native_sculpt_runtime_last_brush_x(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 1 {
        return Err(KainError::runtime(format!("native_sculpt_runtime_last_brush_x expected 1 argument(s), got {}", args.len())));
    }
    let library = unsafe { Library::new(SHARED_LIB_PATH) }
        .map_err(|err| KainError::runtime(format!("Failed to load C shared library {}: {err}", SHARED_LIB_PATH)))?;
    let mut iter = args.into_iter();
    let runtime_handle = extract_c_handle(iter.next().expect("checked arg count"), "void", true)?;
    let symbol: Symbol<unsafe extern "C" fn(*mut std::ffi::c_void) -> std::os::raw::c_int> = unsafe { library.get(&[110, 97, 116, 105, 118, 101, 95, 115, 99, 117, 108, 112, 116, 95, 114, 117, 110, 116, 105, 109, 101, 95, 108, 97, 115, 116, 95, 98, 114, 117, 115, 104, 95, 120, 0]) }
        .map_err(|err| KainError::runtime(format!("Missing C symbol {} in {}: {err}", "native_sculpt_runtime_last_brush_x", SHARED_LIB_PATH)))?;
    let result = unsafe { symbol(runtime_handle) };
    Ok(ToKainValue::to_kain_value(result as i64))
}

fn __kain_c_bridge_native_sculpt_runtime_last_brush_y(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 1 {
        return Err(KainError::runtime(format!("native_sculpt_runtime_last_brush_y expected 1 argument(s), got {}", args.len())));
    }
    let library = unsafe { Library::new(SHARED_LIB_PATH) }
        .map_err(|err| KainError::runtime(format!("Failed to load C shared library {}: {err}", SHARED_LIB_PATH)))?;
    let mut iter = args.into_iter();
    let runtime_handle = extract_c_handle(iter.next().expect("checked arg count"), "void", true)?;
    let symbol: Symbol<unsafe extern "C" fn(*mut std::ffi::c_void) -> std::os::raw::c_int> = unsafe { library.get(&[110, 97, 116, 105, 118, 101, 95, 115, 99, 117, 108, 112, 116, 95, 114, 117, 110, 116, 105, 109, 101, 95, 108, 97, 115, 116, 95, 98, 114, 117, 115, 104, 95, 121, 0]) }
        .map_err(|err| KainError::runtime(format!("Missing C symbol {} in {}: {err}", "native_sculpt_runtime_last_brush_y", SHARED_LIB_PATH)))?;
    let result = unsafe { symbol(runtime_handle) };
    Ok(ToKainValue::to_kain_value(result as i64))
}

fn __kain_c_bridge_native_sculpt_runtime_message_count(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 1 {
        return Err(KainError::runtime(format!("native_sculpt_runtime_message_count expected 1 argument(s), got {}", args.len())));
    }
    let library = unsafe { Library::new(SHARED_LIB_PATH) }
        .map_err(|err| KainError::runtime(format!("Failed to load C shared library {}: {err}", SHARED_LIB_PATH)))?;
    let mut iter = args.into_iter();
    let runtime_handle = extract_c_handle(iter.next().expect("checked arg count"), "void", true)?;
    let symbol: Symbol<unsafe extern "C" fn(*mut std::ffi::c_void) -> std::os::raw::c_int> = unsafe { library.get(&[110, 97, 116, 105, 118, 101, 95, 115, 99, 117, 108, 112, 116, 95, 114, 117, 110, 116, 105, 109, 101, 95, 109, 101, 115, 115, 97, 103, 101, 95, 99, 111, 117, 110, 116, 0]) }
        .map_err(|err| KainError::runtime(format!("Missing C symbol {} in {}: {err}", "native_sculpt_runtime_message_count", SHARED_LIB_PATH)))?;
    let result = unsafe { symbol(runtime_handle) };
    Ok(ToKainValue::to_kain_value(result as i64))
}

fn __kain_c_bridge_native_sculpt_runtime_mouse_move_count(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 1 {
        return Err(KainError::runtime(format!("native_sculpt_runtime_mouse_move_count expected 1 argument(s), got {}", args.len())));
    }
    let library = unsafe { Library::new(SHARED_LIB_PATH) }
        .map_err(|err| KainError::runtime(format!("Failed to load C shared library {}: {err}", SHARED_LIB_PATH)))?;
    let mut iter = args.into_iter();
    let runtime_handle = extract_c_handle(iter.next().expect("checked arg count"), "void", true)?;
    let symbol: Symbol<unsafe extern "C" fn(*mut std::ffi::c_void) -> std::os::raw::c_int> = unsafe { library.get(&[110, 97, 116, 105, 118, 101, 95, 115, 99, 117, 108, 112, 116, 95, 114, 117, 110, 116, 105, 109, 101, 95, 109, 111, 117, 115, 101, 95, 109, 111, 118, 101, 95, 99, 111, 117, 110, 116, 0]) }
        .map_err(|err| KainError::runtime(format!("Missing C symbol {} in {}: {err}", "native_sculpt_runtime_mouse_move_count", SHARED_LIB_PATH)))?;
    let result = unsafe { symbol(runtime_handle) };
    Ok(ToKainValue::to_kain_value(result as i64))
}

fn __kain_c_bridge_native_sculpt_runtime_run(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 3 {
        return Err(KainError::runtime(format!("native_sculpt_runtime_run expected 3 argument(s), got {}", args.len())));
    }
    let library = unsafe { Library::new(SHARED_LIB_PATH) }
        .map_err(|err| KainError::runtime(format!("Failed to load C shared library {}: {err}", SHARED_LIB_PATH)))?;
    let mut iter = args.into_iter();
    let runtime_handle = extract_c_handle(iter.next().expect("checked arg count"), "void", true)?;
    let duration_ms = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))? as std::os::raw::c_int;
    let __capture_bmp_path_owned = <String as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let __capture_bmp_path_cstring = CString::new(__capture_bmp_path_owned).map_err(|_| KainError::runtime("C string argument contained interior NUL".to_string()))?;
    let capture_bmp_path = __capture_bmp_path_cstring.as_ptr();
    let symbol: Symbol<unsafe extern "C" fn(*mut std::ffi::c_void, std::os::raw::c_int, *const std::os::raw::c_char) -> std::os::raw::c_int> = unsafe { library.get(&[110, 97, 116, 105, 118, 101, 95, 115, 99, 117, 108, 112, 116, 95, 114, 117, 110, 116, 105, 109, 101, 95, 114, 117, 110, 0]) }
        .map_err(|err| KainError::runtime(format!("Missing C symbol {} in {}: {err}", "native_sculpt_runtime_run", SHARED_LIB_PATH)))?;
    let result = unsafe { symbol(runtime_handle, duration_ms, capture_bmp_path) };
    Ok(ToKainValue::to_kain_value(result as i64))
}

fn __kain_c_bridge_native_sculpt_runtime_signature(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 1 {
        return Err(KainError::runtime(format!("native_sculpt_runtime_signature expected 1 argument(s), got {}", args.len())));
    }
    let library = unsafe { Library::new(SHARED_LIB_PATH) }
        .map_err(|err| KainError::runtime(format!("Failed to load C shared library {}: {err}", SHARED_LIB_PATH)))?;
    let mut iter = args.into_iter();
    let runtime_handle = extract_c_handle(iter.next().expect("checked arg count"), "void", true)?;
    let symbol: Symbol<unsafe extern "C" fn(*mut std::ffi::c_void) -> *const std::os::raw::c_char> = unsafe { library.get(&[110, 97, 116, 105, 118, 101, 95, 115, 99, 117, 108, 112, 116, 95, 114, 117, 110, 116, 105, 109, 101, 95, 115, 105, 103, 110, 97, 116, 117, 114, 101, 0]) }
        .map_err(|err| KainError::runtime(format!("Missing C symbol {} in {}: {err}", "native_sculpt_runtime_signature", SHARED_LIB_PATH)))?;
    let result = unsafe { symbol(runtime_handle) };
    if result.is_null() { return Err(KainError::runtime("C string return was null".to_string())); }
    let text = unsafe { CStr::from_ptr(result) }.to_string_lossy().into_owned();
    Ok(ToKainValue::to_kain_value(text))
}

fn register_all(env: &mut Env) {
    env.register_native_fn("native_sculpt_runtime_average_fps_x100", __kain_c_bridge_native_sculpt_runtime_average_fps_x100);
    env.register_native_fn("c_native_sculpt_host_native_sculpt_runtime_average_fps_x100", __kain_c_bridge_native_sculpt_runtime_average_fps_x100);
    env.register_native_fn("native_sculpt_runtime_checksum", __kain_c_bridge_native_sculpt_runtime_checksum);
    env.register_native_fn("c_native_sculpt_host_native_sculpt_runtime_checksum", __kain_c_bridge_native_sculpt_runtime_checksum);
    env.register_native_fn("native_sculpt_runtime_create", __kain_c_bridge_native_sculpt_runtime_create);
    env.register_native_fn("c_native_sculpt_host_native_sculpt_runtime_create", __kain_c_bridge_native_sculpt_runtime_create);
    env.register_native_fn("native_sculpt_runtime_destroy", __kain_c_bridge_native_sculpt_runtime_destroy);
    env.register_native_fn("c_native_sculpt_host_native_sculpt_runtime_destroy", __kain_c_bridge_native_sculpt_runtime_destroy);
    env.register_native_fn("native_sculpt_runtime_frame_count", __kain_c_bridge_native_sculpt_runtime_frame_count);
    env.register_native_fn("c_native_sculpt_host_native_sculpt_runtime_frame_count", __kain_c_bridge_native_sculpt_runtime_frame_count);
    env.register_native_fn("native_sculpt_runtime_last_brush_x", __kain_c_bridge_native_sculpt_runtime_last_brush_x);
    env.register_native_fn("c_native_sculpt_host_native_sculpt_runtime_last_brush_x", __kain_c_bridge_native_sculpt_runtime_last_brush_x);
    env.register_native_fn("native_sculpt_runtime_last_brush_y", __kain_c_bridge_native_sculpt_runtime_last_brush_y);
    env.register_native_fn("c_native_sculpt_host_native_sculpt_runtime_last_brush_y", __kain_c_bridge_native_sculpt_runtime_last_brush_y);
    env.register_native_fn("native_sculpt_runtime_message_count", __kain_c_bridge_native_sculpt_runtime_message_count);
    env.register_native_fn("c_native_sculpt_host_native_sculpt_runtime_message_count", __kain_c_bridge_native_sculpt_runtime_message_count);
    env.register_native_fn("native_sculpt_runtime_mouse_move_count", __kain_c_bridge_native_sculpt_runtime_mouse_move_count);
    env.register_native_fn("c_native_sculpt_host_native_sculpt_runtime_mouse_move_count", __kain_c_bridge_native_sculpt_runtime_mouse_move_count);
    env.register_native_fn("native_sculpt_runtime_run", __kain_c_bridge_native_sculpt_runtime_run);
    env.register_native_fn("c_native_sculpt_host_native_sculpt_runtime_run", __kain_c_bridge_native_sculpt_runtime_run);
    env.register_native_fn("native_sculpt_runtime_signature", __kain_c_bridge_native_sculpt_runtime_signature);
    env.register_native_fn("c_native_sculpt_host_native_sculpt_runtime_signature", __kain_c_bridge_native_sculpt_runtime_signature);
}

#[no_mangle]
pub extern "C" fn kain_register_bridge(env: *mut Env) {
    let Some(env) = (unsafe { env.as_mut() }) else {
        return;
    };
    register_all(env);
}
