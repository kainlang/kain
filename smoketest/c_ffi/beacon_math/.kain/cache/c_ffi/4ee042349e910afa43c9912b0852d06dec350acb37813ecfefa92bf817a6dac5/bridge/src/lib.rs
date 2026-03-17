use kain_core::error::KainError;
use kain_core::runtime::{Env, Value};
use kain_host::{FromKainValue, ToKainValue};
use libloading::{Library, Symbol};
use std::ffi::{CStr, CString};

const SHARED_LIB_PATH: &str = "M:\\Code\\Kain\\smoketest\\c_ffi\\beacon_math\\native/beacon_math.dll";

fn __kain_c_bridge_beacon_add(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 2 {
        return Err(KainError::runtime(format!("beacon_add expected 2 argument(s), got {}", args.len())));
    }
    let library = unsafe { Library::new(SHARED_LIB_PATH) }.map_err(|err| KainError::runtime(format!("Failed to load C shared library '{}': {err}", SHARED_LIB_PATH)))?;
    let mut iter = args.into_iter();
    let a = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))? as std::os::raw::c_int;
    let b = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))? as std::os::raw::c_int;
    let symbol: Symbol<unsafe extern "C" fn(std::os::raw::c_int, std::os::raw::c_int) -> std::os::raw::c_int> = unsafe { library.get(&[98, 101, 97, 99, 111, 110, 95, 97, 100, 100, 0]) }.map_err(|err| KainError::runtime(format!("Missing C symbol 'beacon_add' in '{}': {err}", SHARED_LIB_PATH)))?;
    let result = unsafe { symbol(a, b) };
    Ok(ToKainValue::to_kain_value(result as i64))
}

fn __kain_c_bridge_beacon_is_even(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 1 {
        return Err(KainError::runtime(format!("beacon_is_even expected 1 argument(s), got {}", args.len())));
    }
    let library = unsafe { Library::new(SHARED_LIB_PATH) }.map_err(|err| KainError::runtime(format!("Failed to load C shared library '{}': {err}", SHARED_LIB_PATH)))?;
    let mut iter = args.into_iter();
    let value = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))? as std::os::raw::c_int;
    let symbol: Symbol<unsafe extern "C" fn(std::os::raw::c_int) -> bool> = unsafe { library.get(&[98, 101, 97, 99, 111, 110, 95, 105, 115, 95, 101, 118, 101, 110, 0]) }.map_err(|err| KainError::runtime(format!("Missing C symbol 'beacon_is_even' in '{}': {err}", SHARED_LIB_PATH)))?;
    let result = unsafe { symbol(value) };
    Ok(ToKainValue::to_kain_value(result))
}

fn __kain_c_bridge_beacon_scale(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 2 {
        return Err(KainError::runtime(format!("beacon_scale expected 2 argument(s), got {}", args.len())));
    }
    let library = unsafe { Library::new(SHARED_LIB_PATH) }.map_err(|err| KainError::runtime(format!("Failed to load C shared library '{}': {err}", SHARED_LIB_PATH)))?;
    let mut iter = args.into_iter();
    let value = <f64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let factor = <f64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let symbol: Symbol<unsafe extern "C" fn(f64, f64) -> f64> = unsafe { library.get(&[98, 101, 97, 99, 111, 110, 95, 115, 99, 97, 108, 101, 0]) }.map_err(|err| KainError::runtime(format!("Missing C symbol 'beacon_scale' in '{}': {err}", SHARED_LIB_PATH)))?;
    let result = unsafe { symbol(value, factor) };
    Ok(ToKainValue::to_kain_value(result as f64))
}

fn __kain_c_bridge_beacon_label(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 1 {
        return Err(KainError::runtime(format!("beacon_label expected 1 argument(s), got {}", args.len())));
    }
    let library = unsafe { Library::new(SHARED_LIB_PATH) }.map_err(|err| KainError::runtime(format!("Failed to load C shared library '{}': {err}", SHARED_LIB_PATH)))?;
    let mut iter = args.into_iter();
    let id = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))? as std::os::raw::c_int;
    let symbol: Symbol<unsafe extern "C" fn(std::os::raw::c_int) -> *const std::os::raw::c_char> = unsafe { library.get(&[98, 101, 97, 99, 111, 110, 95, 108, 97, 98, 101, 108, 0]) }.map_err(|err| KainError::runtime(format!("Missing C symbol 'beacon_label' in '{}': {err}", SHARED_LIB_PATH)))?;
    let result = unsafe { symbol(id) };
    if result.is_null() { return Err(KainError::runtime("C string return was null".to_string())); }
    let text = unsafe { CStr::from_ptr(result) }.to_string_lossy().into_owned();
    Ok(ToKainValue::to_kain_value(text))
}

fn register_all(env: &mut Env) {
    env.register_native_fn("beacon_add", __kain_c_bridge_beacon_add);
    env.register_native_fn("c_beacon_math_beacon_add", __kain_c_bridge_beacon_add);
    env.register_native_fn("beacon_is_even", __kain_c_bridge_beacon_is_even);
    env.register_native_fn("c_beacon_math_beacon_is_even", __kain_c_bridge_beacon_is_even);
    env.register_native_fn("beacon_scale", __kain_c_bridge_beacon_scale);
    env.register_native_fn("c_beacon_math_beacon_scale", __kain_c_bridge_beacon_scale);
    env.register_native_fn("beacon_label", __kain_c_bridge_beacon_label);
    env.register_native_fn("c_beacon_math_beacon_label", __kain_c_bridge_beacon_label);
}

#[no_mangle]
pub extern "C" fn kain_register_bridge(env: *mut Env) {
    let Some(env) = (unsafe { env.as_mut() }) else {
        return;
    };
    register_all(env);
}
