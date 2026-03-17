use kain_core::error::KainError;
use kain_core::runtime::{Env, Value};
use kain_host::{FromKainValue, ToKainValue};
use libloading::{Library, Symbol};
use std::ffi::{c_void, CStr, CString};
use std::sync::{Arc, RwLock};

const SHARED_LIB_PATH: &str = "M:\\Code\\Kain\\smoketest\\c_ffi\\miniaudio_tone_lab\\native/audio_tone_lab.dll";

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

fn __kain_c_bridge_audiofx_wav_channels(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 1 {
        return Err(KainError::runtime(format!("audiofx_wav_channels expected 1 argument(s), got {}", args.len())));
    }
    let library = unsafe { Library::new(SHARED_LIB_PATH) }
        .map_err(|err| KainError::runtime(format!("Failed to load C shared library {}: {err}", SHARED_LIB_PATH)))?;
    let mut iter = args.into_iter();
    let __path_owned = <String as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let __path_cstring = CString::new(__path_owned).map_err(|_| KainError::runtime("C string argument contained interior NUL".to_string()))?;
    let path = __path_cstring.as_ptr();
    let symbol: Symbol<unsafe extern "C" fn(*const std::os::raw::c_char) -> std::os::raw::c_int> = unsafe { library.get(&[97, 117, 100, 105, 111, 102, 120, 95, 119, 97, 118, 95, 99, 104, 97, 110, 110, 101, 108, 115, 0]) }
        .map_err(|err| KainError::runtime(format!("Missing C symbol {} in {}: {err}", "audiofx_wav_channels", SHARED_LIB_PATH)))?;
    let result = unsafe { symbol(path) };
    Ok(ToKainValue::to_kain_value(result as i64))
}

fn __kain_c_bridge_audiofx_wav_frame_count(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 1 {
        return Err(KainError::runtime(format!("audiofx_wav_frame_count expected 1 argument(s), got {}", args.len())));
    }
    let library = unsafe { Library::new(SHARED_LIB_PATH) }
        .map_err(|err| KainError::runtime(format!("Failed to load C shared library {}: {err}", SHARED_LIB_PATH)))?;
    let mut iter = args.into_iter();
    let __path_owned = <String as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let __path_cstring = CString::new(__path_owned).map_err(|_| KainError::runtime("C string argument contained interior NUL".to_string()))?;
    let path = __path_cstring.as_ptr();
    let symbol: Symbol<unsafe extern "C" fn(*const std::os::raw::c_char) -> std::os::raw::c_int> = unsafe { library.get(&[97, 117, 100, 105, 111, 102, 120, 95, 119, 97, 118, 95, 102, 114, 97, 109, 101, 95, 99, 111, 117, 110, 116, 0]) }
        .map_err(|err| KainError::runtime(format!("Missing C symbol {} in {}: {err}", "audiofx_wav_frame_count", SHARED_LIB_PATH)))?;
    let result = unsafe { symbol(path) };
    Ok(ToKainValue::to_kain_value(result as i64))
}

fn __kain_c_bridge_audiofx_wav_peak(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 1 {
        return Err(KainError::runtime(format!("audiofx_wav_peak expected 1 argument(s), got {}", args.len())));
    }
    let library = unsafe { Library::new(SHARED_LIB_PATH) }
        .map_err(|err| KainError::runtime(format!("Failed to load C shared library {}: {err}", SHARED_LIB_PATH)))?;
    let mut iter = args.into_iter();
    let __path_owned = <String as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let __path_cstring = CString::new(__path_owned).map_err(|_| KainError::runtime("C string argument contained interior NUL".to_string()))?;
    let path = __path_cstring.as_ptr();
    let symbol: Symbol<unsafe extern "C" fn(*const std::os::raw::c_char) -> f64> = unsafe { library.get(&[97, 117, 100, 105, 111, 102, 120, 95, 119, 97, 118, 95, 112, 101, 97, 107, 0]) }
        .map_err(|err| KainError::runtime(format!("Missing C symbol {} in {}: {err}", "audiofx_wav_peak", SHARED_LIB_PATH)))?;
    let result = unsafe { symbol(path) };
    Ok(ToKainValue::to_kain_value(result as f64))
}

fn __kain_c_bridge_audiofx_wav_signature(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 1 {
        return Err(KainError::runtime(format!("audiofx_wav_signature expected 1 argument(s), got {}", args.len())));
    }
    let library = unsafe { Library::new(SHARED_LIB_PATH) }
        .map_err(|err| KainError::runtime(format!("Failed to load C shared library {}: {err}", SHARED_LIB_PATH)))?;
    let mut iter = args.into_iter();
    let __path_owned = <String as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let __path_cstring = CString::new(__path_owned).map_err(|_| KainError::runtime("C string argument contained interior NUL".to_string()))?;
    let path = __path_cstring.as_ptr();
    let symbol: Symbol<unsafe extern "C" fn(*const std::os::raw::c_char) -> *const std::os::raw::c_char> = unsafe { library.get(&[97, 117, 100, 105, 111, 102, 120, 95, 119, 97, 118, 95, 115, 105, 103, 110, 97, 116, 117, 114, 101, 0]) }
        .map_err(|err| KainError::runtime(format!("Missing C symbol {} in {}: {err}", "audiofx_wav_signature", SHARED_LIB_PATH)))?;
    let result = unsafe { symbol(path) };
    if result.is_null() { return Err(KainError::runtime("C string return was null".to_string())); }
    let text = unsafe { CStr::from_ptr(result) }.to_string_lossy().into_owned();
    Ok(ToKainValue::to_kain_value(text))
}

fn __kain_c_bridge_audiofx_write_sine_wave(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 6 {
        return Err(KainError::runtime(format!("audiofx_write_sine_wave expected 6 argument(s), got {}", args.len())));
    }
    let library = unsafe { Library::new(SHARED_LIB_PATH) }
        .map_err(|err| KainError::runtime(format!("Failed to load C shared library {}: {err}", SHARED_LIB_PATH)))?;
    let mut iter = args.into_iter();
    let __path_owned = <String as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let __path_cstring = CString::new(__path_owned).map_err(|_| KainError::runtime("C string argument contained interior NUL".to_string()))?;
    let path = __path_cstring.as_ptr();
    let sample_rate = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))? as std::os::raw::c_int;
    let channels = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))? as std::os::raw::c_int;
    let duration_ms = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))? as std::os::raw::c_int;
    let frequency_hz = <f64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let amplitude = <f64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let symbol: Symbol<unsafe extern "C" fn(*const std::os::raw::c_char, std::os::raw::c_int, std::os::raw::c_int, std::os::raw::c_int, f64, f64) -> std::os::raw::c_int> = unsafe { library.get(&[97, 117, 100, 105, 111, 102, 120, 95, 119, 114, 105, 116, 101, 95, 115, 105, 110, 101, 95, 119, 97, 118, 101, 0]) }
        .map_err(|err| KainError::runtime(format!("Missing C symbol {} in {}: {err}", "audiofx_write_sine_wave", SHARED_LIB_PATH)))?;
    let result = unsafe { symbol(path, sample_rate, channels, duration_ms, frequency_hz, amplitude) };
    Ok(ToKainValue::to_kain_value(result as i64))
}

fn register_all(env: &mut Env) {
    env.register_native_fn("audiofx_wav_channels", __kain_c_bridge_audiofx_wav_channels);
    env.register_native_fn("c_audio_tone_lab_audiofx_wav_channels", __kain_c_bridge_audiofx_wav_channels);
    env.register_native_fn("audiofx_wav_frame_count", __kain_c_bridge_audiofx_wav_frame_count);
    env.register_native_fn("c_audio_tone_lab_audiofx_wav_frame_count", __kain_c_bridge_audiofx_wav_frame_count);
    env.register_native_fn("audiofx_wav_peak", __kain_c_bridge_audiofx_wav_peak);
    env.register_native_fn("c_audio_tone_lab_audiofx_wav_peak", __kain_c_bridge_audiofx_wav_peak);
    env.register_native_fn("audiofx_wav_signature", __kain_c_bridge_audiofx_wav_signature);
    env.register_native_fn("c_audio_tone_lab_audiofx_wav_signature", __kain_c_bridge_audiofx_wav_signature);
    env.register_native_fn("audiofx_write_sine_wave", __kain_c_bridge_audiofx_write_sine_wave);
    env.register_native_fn("c_audio_tone_lab_audiofx_write_sine_wave", __kain_c_bridge_audiofx_write_sine_wave);
}

#[no_mangle]
pub extern "C" fn kain_register_bridge(env: *mut Env) {
    let Some(env) = (unsafe { env.as_mut() }) else {
        return;
    };
    register_all(env);
}
