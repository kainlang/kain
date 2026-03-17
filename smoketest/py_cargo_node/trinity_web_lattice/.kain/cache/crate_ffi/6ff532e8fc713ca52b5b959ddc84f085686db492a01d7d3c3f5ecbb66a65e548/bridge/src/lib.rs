use kain_core::error::KainError;
use kain_core::runtime::{Env, Value};
use kain_host::{FromKainValue, ToKainValue};

fn __kain_bridge_cargo_spokes(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 3 {
        return Err(KainError::runtime(format!("cargo_spokes expected 3 argument(s), got {}", args.len())));
    }
    let mut iter = args.into_iter();
    let width = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let count = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let phase = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let result = trinity_stack_node::cargo_spokes(width, count, phase);
    Ok(ToKainValue::to_kain_value(result))
}

fn __kain_bridge_cargo_markers(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 3 {
        return Err(KainError::runtime(format!("cargo_markers expected 3 argument(s), got {}", args.len())));
    }
    let mut iter = args.into_iter();
    let width = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let height = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let phase = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let result = trinity_stack_node::cargo_markers(width, height, phase);
    Ok(ToKainValue::to_kain_value(result))
}

fn __kain_bridge_trinity_signature(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 2 {
        return Err(KainError::runtime(format!("trinity_signature expected 2 argument(s), got {}", args.len())));
    }
    let mut iter = args.into_iter();
    let __label_owned = <String as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let label = __label_owned.as_str();
    let phase = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let result = trinity_stack_node::trinity_signature(label, phase);
    Ok(ToKainValue::to_kain_value(result))
}

fn __kain_bridge_trinity_stamp_orbit(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 2 {
        return Err(KainError::runtime(format!("trinity_stamp_orbit expected 2 argument(s), got {}", args.len())));
    }
    let mut iter = args.into_iter();
    let seed = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let stride = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let result = trinity_stack_node::TrinityStamp::orbit(seed, stride);
    Ok(ToKainValue::to_kain_value(result))
}

fn register_all(env: &mut Env) {
    env.register_native_fn("cargo_spokes", __kain_bridge_cargo_spokes);
    env.register_native_fn("rust_trinity_stack_node_cargo_spokes", __kain_bridge_cargo_spokes);
    env.register_native_fn("cargo_markers", __kain_bridge_cargo_markers);
    env.register_native_fn("rust_trinity_stack_node_cargo_markers", __kain_bridge_cargo_markers);
    env.register_native_fn("trinity_signature", __kain_bridge_trinity_signature);
    env.register_native_fn("rust_trinity_stack_node_trinity_signature", __kain_bridge_trinity_signature);
    env.register_native_fn("trinity_stamp_orbit", __kain_bridge_trinity_stamp_orbit);
    env.register_native_fn("rust_trinity_stack_node_trinity_stamp_orbit", __kain_bridge_trinity_stamp_orbit);
}

#[no_mangle]
pub extern "C" fn kain_register_bridge(env: *mut Env) {
    let Some(env) = (unsafe { env.as_mut() }) else {
        return;
    };
    register_all(env);
}
