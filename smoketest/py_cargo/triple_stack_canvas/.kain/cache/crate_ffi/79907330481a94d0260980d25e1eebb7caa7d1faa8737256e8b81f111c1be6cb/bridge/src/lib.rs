use kain_core::error::KainError;
use kain_core::runtime::{Env, Value};
use kain_host::{FromKainValue, ToKainValue};

fn __kain_bridge_cargo_signature(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 2 {
        return Err(KainError::runtime(format!("cargo_signature expected 2 argument(s), got {}", args.len())));
    }
    let mut iter = args.into_iter();
    let __label_owned = <String as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let label = __label_owned.as_str();
    let phase = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let result = py_cargo_canvas::cargo_signature(label, phase);
    Ok(ToKainValue::to_kain_value(result))
}

fn __kain_bridge_cargo_energy(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 1 {
        return Err(KainError::runtime(format!("cargo_energy expected 1 argument(s), got {}", args.len())));
    }
    let mut iter = args.into_iter();
    let values = <Vec<i64> as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let result = py_cargo_canvas::cargo_energy(values);
    Ok(ToKainValue::to_kain_value(result))
}

fn __kain_bridge_cargo_mask_row(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 3 {
        return Err(KainError::runtime(format!("cargo_mask_row expected 3 argument(s), got {}", args.len())));
    }
    let mut iter = args.into_iter();
    let width = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let row = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let phase = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let result = py_cargo_canvas::cargo_mask_row(width, row, phase);
    Ok(ToKainValue::to_kain_value(result))
}

fn __kain_bridge_cargo_beacon_points(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 3 {
        return Err(KainError::runtime(format!("cargo_beacon_points expected 3 argument(s), got {}", args.len())));
    }
    let mut iter = args.into_iter();
    let width = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let height = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let phase = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let result = py_cargo_canvas::cargo_beacon_points(width, height, phase);
    Ok(ToKainValue::to_kain_value(result))
}

fn __kain_bridge_hybrid_stamp_orbit(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 2 {
        return Err(KainError::runtime(format!("hybrid_stamp_orbit expected 2 argument(s), got {}", args.len())));
    }
    let mut iter = args.into_iter();
    let seed = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let lanes = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let result = py_cargo_canvas::HybridStamp::orbit(seed, lanes);
    Ok(ToKainValue::to_kain_value(result))
}

fn register_all(env: &mut Env) {
    env.register_native_fn("cargo_signature", __kain_bridge_cargo_signature);
    env.register_native_fn("rust_py_cargo_canvas_cargo_signature", __kain_bridge_cargo_signature);
    env.register_native_fn("cargo_energy", __kain_bridge_cargo_energy);
    env.register_native_fn("rust_py_cargo_canvas_cargo_energy", __kain_bridge_cargo_energy);
    env.register_native_fn("cargo_mask_row", __kain_bridge_cargo_mask_row);
    env.register_native_fn("rust_py_cargo_canvas_cargo_mask_row", __kain_bridge_cargo_mask_row);
    env.register_native_fn("cargo_beacon_points", __kain_bridge_cargo_beacon_points);
    env.register_native_fn("rust_py_cargo_canvas_cargo_beacon_points", __kain_bridge_cargo_beacon_points);
    env.register_native_fn("hybrid_stamp_orbit", __kain_bridge_hybrid_stamp_orbit);
    env.register_native_fn("rust_py_cargo_canvas_hybrid_stamp_orbit", __kain_bridge_hybrid_stamp_orbit);
}

#[no_mangle]
pub extern "C" fn kain_register_bridge(env: *mut Env) {
    let Some(env) = (unsafe { env.as_mut() }) else {
        return;
    };
    register_all(env);
}
