use kain_core::error::KainError;
use kain_core::runtime::{Env, Value};
use kain_host::{FromKainValue, ToKainValue};
use libloading::{Library, Symbol};
use std::ffi::{c_void, CStr, CString};
use std::sync::{Arc, RwLock};

const SHARED_LIB_PATH: &str = "M:\\Code\\Kain\\smoketest\\py_cargo_node_c\\quad_prism_halo\\native/image_fx.dll";

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

fn __kain_c_bridge_imagefx_checksum(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 2 {
        return Err(KainError::runtime(format!("imagefx_checksum expected 2 argument(s), got {}", args.len())));
    }
    let library = unsafe { Library::new(SHARED_LIB_PATH) }
        .map_err(|err| KainError::runtime(format!("Failed to load C shared library {}: {err}", SHARED_LIB_PATH)))?;
    let mut iter = args.into_iter();
    let __pixels_buffer = ByteBufferArg::from_value(_env, iter.next().expect("checked arg count"), false)?;
    let pixels = __pixels_buffer.as_ptr();
    let __len_value = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    if __len_value < 0 { return Err(KainError::runtime("unsigned C ABI argument cannot be negative".to_string())); }
    let len = __len_value as usize;
    let symbol: Symbol<unsafe extern "C" fn(*const u8, usize) -> u64> = unsafe { library.get(&[105, 109, 97, 103, 101, 102, 120, 95, 99, 104, 101, 99, 107, 115, 117, 109, 0]) }
        .map_err(|err| KainError::runtime(format!("Missing C symbol {} in {}: {err}", "imagefx_checksum", SHARED_LIB_PATH)))?;
    let result = unsafe { symbol(pixels, len) };
    if (result as u128) > (i64::MAX as u128) { return Err(KainError::runtime("unsigned C ABI return overflowed Kain Int".to_string())); }
    Ok(ToKainValue::to_kain_value(result as i64))
}

fn __kain_c_bridge_imagefx_halo_rgba(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 3 {
        return Err(KainError::runtime(format!("imagefx_halo_rgba expected 3 argument(s), got {}", args.len())));
    }
    let library = unsafe { Library::new(SHARED_LIB_PATH) }
        .map_err(|err| KainError::runtime(format!("Failed to load C shared library {}: {err}", SHARED_LIB_PATH)))?;
    let mut iter = args.into_iter();
    let mut __pixels_buffer = ByteBufferArg::from_value(_env, iter.next().expect("checked arg count"), true)?;
    let pixels = __pixels_buffer.as_mut_ptr();
    let __len_value = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    if __len_value < 0 { return Err(KainError::runtime("unsigned C ABI argument cannot be negative".to_string())); }
    let len = __len_value as usize;
    let accent = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))? as std::os::raw::c_int;
    let symbol: Symbol<unsafe extern "C" fn(*mut u8, usize, std::os::raw::c_int) -> ()> = unsafe { library.get(&[105, 109, 97, 103, 101, 102, 120, 95, 104, 97, 108, 111, 95, 114, 103, 98, 97, 0]) }
        .map_err(|err| KainError::runtime(format!("Missing C symbol {} in {}: {err}", "imagefx_halo_rgba", SHARED_LIB_PATH)))?;
    let result = unsafe { symbol(pixels, len, accent) };
    __pixels_buffer.commit(_env)?;
    Ok(Value::Unit)
}

fn __kain_c_bridge_imagefx_signature(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 4 {
        return Err(KainError::runtime(format!("imagefx_signature expected 4 argument(s), got {}", args.len())));
    }
    let library = unsafe { Library::new(SHARED_LIB_PATH) }
        .map_err(|err| KainError::runtime(format!("Failed to load C shared library {}: {err}", SHARED_LIB_PATH)))?;
    let mut iter = args.into_iter();
    let width = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))? as std::os::raw::c_int;
    let height = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))? as std::os::raw::c_int;
    let __checksum_value = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    if __checksum_value < 0 { return Err(KainError::runtime("unsigned C ABI argument cannot be negative".to_string())); }
    let checksum = __checksum_value as u64;
    let accent = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))? as std::os::raw::c_int;
    let symbol: Symbol<unsafe extern "C" fn(std::os::raw::c_int, std::os::raw::c_int, u64, std::os::raw::c_int) -> *const std::os::raw::c_char> = unsafe { library.get(&[105, 109, 97, 103, 101, 102, 120, 95, 115, 105, 103, 110, 97, 116, 117, 114, 101, 0]) }
        .map_err(|err| KainError::runtime(format!("Missing C symbol {} in {}: {err}", "imagefx_signature", SHARED_LIB_PATH)))?;
    let result = unsafe { symbol(width, height, checksum, accent) };
    if result.is_null() { return Err(KainError::runtime("C string return was null".to_string())); }
    let text = unsafe { CStr::from_ptr(result) }.to_string_lossy().into_owned();
    Ok(ToKainValue::to_kain_value(text))
}

fn __kain_c_bridge_imagefx_workspace_create(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 2 {
        return Err(KainError::runtime(format!("imagefx_workspace_create expected 2 argument(s), got {}", args.len())));
    }
    let library = unsafe { Library::new(SHARED_LIB_PATH) }
        .map_err(|err| KainError::runtime(format!("Failed to load C shared library {}: {err}", SHARED_LIB_PATH)))?;
    let mut iter = args.into_iter();
    let width = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))? as std::os::raw::c_int;
    let height = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))? as std::os::raw::c_int;
    let symbol: Symbol<unsafe extern "C" fn(std::os::raw::c_int, std::os::raw::c_int) -> *mut std::ffi::c_void> = unsafe { library.get(&[105, 109, 97, 103, 101, 102, 120, 95, 119, 111, 114, 107, 115, 112, 97, 99, 101, 95, 99, 114, 101, 97, 116, 101, 0]) }
        .map_err(|err| KainError::runtime(format!("Missing C symbol {} in {}: {err}", "imagefx_workspace_create", SHARED_LIB_PATH)))?;
    let result = unsafe { symbol(width, height) };
    if result.is_null() { return Ok(Value::None); }
    Ok(Value::host_object("kain.c.handle", Arc::new(CAbiOpaqueHandle { pointee: "ImageWorkspace".to_string(), mutable: true, address: result as usize })))
}

fn __kain_c_bridge_imagefx_workspace_area(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 1 {
        return Err(KainError::runtime(format!("imagefx_workspace_area expected 1 argument(s), got {}", args.len())));
    }
    let library = unsafe { Library::new(SHARED_LIB_PATH) }
        .map_err(|err| KainError::runtime(format!("Failed to load C shared library {}: {err}", SHARED_LIB_PATH)))?;
    let mut iter = args.into_iter();
    let workspace = extract_c_handle(iter.next().expect("checked arg count"), "ImageWorkspace", true)?;
    let symbol: Symbol<unsafe extern "C" fn(*mut std::ffi::c_void) -> std::os::raw::c_int> = unsafe { library.get(&[105, 109, 97, 103, 101, 102, 120, 95, 119, 111, 114, 107, 115, 112, 97, 99, 101, 95, 97, 114, 101, 97, 0]) }
        .map_err(|err| KainError::runtime(format!("Missing C symbol {} in {}: {err}", "imagefx_workspace_area", SHARED_LIB_PATH)))?;
    let result = unsafe { symbol(workspace) };
    Ok(ToKainValue::to_kain_value(result as i64))
}

fn __kain_c_bridge_imagefx_workspace_destroy(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 1 {
        return Err(KainError::runtime(format!("imagefx_workspace_destroy expected 1 argument(s), got {}", args.len())));
    }
    let library = unsafe { Library::new(SHARED_LIB_PATH) }
        .map_err(|err| KainError::runtime(format!("Failed to load C shared library {}: {err}", SHARED_LIB_PATH)))?;
    let mut iter = args.into_iter();
    let workspace = extract_c_handle(iter.next().expect("checked arg count"), "ImageWorkspace", true)?;
    let symbol: Symbol<unsafe extern "C" fn(*mut std::ffi::c_void) -> ()> = unsafe { library.get(&[105, 109, 97, 103, 101, 102, 120, 95, 119, 111, 114, 107, 115, 112, 97, 99, 101, 95, 100, 101, 115, 116, 114, 111, 121, 0]) }
        .map_err(|err| KainError::runtime(format!("Missing C symbol {} in {}: {err}", "imagefx_workspace_destroy", SHARED_LIB_PATH)))?;
    let result = unsafe { symbol(workspace) };
    Ok(Value::Unit)
}

fn register_all(env: &mut Env) {
    env.register_native_fn("imagefx_checksum", __kain_c_bridge_imagefx_checksum);
    env.register_native_fn("c_image_fx_imagefx_checksum", __kain_c_bridge_imagefx_checksum);
    env.register_native_fn("imagefx_halo_rgba", __kain_c_bridge_imagefx_halo_rgba);
    env.register_native_fn("c_image_fx_imagefx_halo_rgba", __kain_c_bridge_imagefx_halo_rgba);
    env.register_native_fn("imagefx_signature", __kain_c_bridge_imagefx_signature);
    env.register_native_fn("c_image_fx_imagefx_signature", __kain_c_bridge_imagefx_signature);
    env.register_native_fn("imagefx_workspace_create", __kain_c_bridge_imagefx_workspace_create);
    env.register_native_fn("c_image_fx_imagefx_workspace_create", __kain_c_bridge_imagefx_workspace_create);
    env.register_native_fn("imagefx_workspace_area", __kain_c_bridge_imagefx_workspace_area);
    env.register_native_fn("c_image_fx_imagefx_workspace_area", __kain_c_bridge_imagefx_workspace_area);
    env.register_native_fn("imagefx_workspace_destroy", __kain_c_bridge_imagefx_workspace_destroy);
    env.register_native_fn("c_image_fx_imagefx_workspace_destroy", __kain_c_bridge_imagefx_workspace_destroy);
}

#[no_mangle]
pub extern "C" fn kain_register_bridge(env: *mut Env) {
    let Some(env) = (unsafe { env.as_mut() }) else {
        return;
    };
    register_all(env);
}
