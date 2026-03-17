use kain_core::error::KainError;
use kain_core::runtime::{Env, Value};
use kain_host::{FromKainValue, ToKainValue};
use sample_cli_ffi as target_crate;

fn __kain_bridge_add(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 2 {
        return Err(KainError::runtime(format!("add expected 2 argument(s), got {}", args.len())));
    }
    let mut iter = args.into_iter();
    let a = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let b = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let result = sample_cli_ffi::add(a, b);
    Ok(ToKainValue::to_kain_value(result))
}

fn register_all(env: &mut Env) {
    env.register_native_fn("add", __kain_bridge_add);
    env.register_native_fn("rust_sample_cli_ffi_add", __kain_bridge_add);
}

#[no_mangle]
pub extern "C" fn kain_register_bridge(env: *mut Env) {
    let Some(env) = (unsafe { env.as_mut() }) else {
        return;
    };
    register_all(env);
}
