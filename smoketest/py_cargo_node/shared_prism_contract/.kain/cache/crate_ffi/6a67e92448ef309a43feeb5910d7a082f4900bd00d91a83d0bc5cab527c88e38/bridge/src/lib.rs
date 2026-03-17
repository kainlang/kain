use kain_core::error::KainError;
use kain_core::runtime::{Env, Value};
use kain_host::{FromKainValue, ToKainValue};

fn __kain_bridge_shared_prism_checksum(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 1 {
        return Err(KainError::runtime(format!("shared_prism_checksum expected 1 argument(s), got {}", args.len())));
    }
    let mut iter = args.into_iter();
    let bytes = <Vec<i64> as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let result = shared_prism_lab::shared_prism_checksum(bytes);
    Ok(ToKainValue::to_kain_value(result))
}

fn __kain_bridge_shared_prism_bands(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 2 {
        return Err(KainError::runtime(format!("shared_prism_bands expected 2 argument(s), got {}", args.len())));
    }
    let mut iter = args.into_iter();
    let width = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let count = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let result = shared_prism_lab::shared_prism_bands(width, count);
    Ok(ToKainValue::to_kain_value(result))
}

fn __kain_bridge_shared_prism_signature(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 4 {
        return Err(KainError::runtime(format!("shared_prism_signature expected 4 argument(s), got {}", args.len())));
    }
    let mut iter = args.into_iter();
    let label = <String as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let width = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let height = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let checksum = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let result = shared_prism_lab::shared_prism_signature(label, width, height, checksum);
    Ok(ToKainValue::to_kain_value(result))
}

fn register_all(env: &mut Env) {
    env.register_native_fn("shared_prism_checksum", __kain_bridge_shared_prism_checksum);
    env.register_native_fn("rust_shared_prism_lab_shared_prism_checksum", __kain_bridge_shared_prism_checksum);
    env.register_native_fn("shared_prism_bands", __kain_bridge_shared_prism_bands);
    env.register_native_fn("rust_shared_prism_lab_shared_prism_bands", __kain_bridge_shared_prism_bands);
    env.register_native_fn("shared_prism_signature", __kain_bridge_shared_prism_signature);
    env.register_native_fn("rust_shared_prism_lab_shared_prism_signature", __kain_bridge_shared_prism_signature);
}

#[no_mangle]
pub extern "C" fn kain_register_bridge(env: *mut Env) {
    let Some(env) = (unsafe { env.as_mut() }) else {
        return;
    };
    register_all(env);
}
