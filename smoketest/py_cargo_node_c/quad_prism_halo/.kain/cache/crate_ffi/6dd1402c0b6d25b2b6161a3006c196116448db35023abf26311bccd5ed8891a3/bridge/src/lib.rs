use kain_core::error::KainError;
use kain_core::runtime::{Env, Value};
use kain_host::{FromKainValue, ToKainValue};

fn __kain_bridge_quad_prism_checksum(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 1 {
        return Err(KainError::runtime(format!("quad_prism_checksum expected 1 argument(s), got {}", args.len())));
    }
    let mut iter = args.into_iter();
    let bytes = <Vec<i64> as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let result = quad_prism_lab::quad_prism_checksum(bytes);
    Ok(ToKainValue::to_kain_value(result))
}

fn __kain_bridge_quad_prism_bands(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 2 {
        return Err(KainError::runtime(format!("quad_prism_bands expected 2 argument(s), got {}", args.len())));
    }
    let mut iter = args.into_iter();
    let width = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let count = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let result = quad_prism_lab::quad_prism_bands(width, count);
    Ok(ToKainValue::to_kain_value(result))
}

fn __kain_bridge_quad_prism_signature(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 5 {
        return Err(KainError::runtime(format!("quad_prism_signature expected 5 argument(s), got {}", args.len())));
    }
    let mut iter = args.into_iter();
    let label = <String as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let width = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let height = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let rust_checksum = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let c_checksum = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let result = quad_prism_lab::quad_prism_signature(label, width, height, rust_checksum, c_checksum);
    Ok(ToKainValue::to_kain_value(result))
}

fn __kain_bridge_quad_prism_phase_stamp(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 3 {
        return Err(KainError::runtime(format!("quad_prism_phase_stamp expected 3 argument(s), got {}", args.len())));
    }
    let mut iter = args.into_iter();
    let width = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let height = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let phase = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let result = quad_prism_lab::quad_prism_phase_stamp(width, height, phase);
    Ok(ToKainValue::to_kain_value(result))
}

fn register_all(env: &mut Env) {
    env.register_native_fn("quad_prism_checksum", __kain_bridge_quad_prism_checksum);
    env.register_native_fn("rust_quad_prism_lab_quad_prism_checksum", __kain_bridge_quad_prism_checksum);
    env.register_native_fn("quad_prism_bands", __kain_bridge_quad_prism_bands);
    env.register_native_fn("rust_quad_prism_lab_quad_prism_bands", __kain_bridge_quad_prism_bands);
    env.register_native_fn("quad_prism_signature", __kain_bridge_quad_prism_signature);
    env.register_native_fn("rust_quad_prism_lab_quad_prism_signature", __kain_bridge_quad_prism_signature);
    env.register_native_fn("quad_prism_phase_stamp", __kain_bridge_quad_prism_phase_stamp);
    env.register_native_fn("rust_quad_prism_lab_quad_prism_phase_stamp", __kain_bridge_quad_prism_phase_stamp);
}

#[no_mangle]
pub extern "C" fn kain_register_bridge(env: *mut Env) {
    let Some(env) = (unsafe { env.as_mut() }) else {
        return;
    };
    register_all(env);
}
