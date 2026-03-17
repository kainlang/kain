use kain_core::error::KainError;
use kain_core::runtime::{Env, Value};
use kain_host::{FromKainValue, ToKainValue};

fn __kain_bridge_signal_bands(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 2 {
        return Err(KainError::runtime(format!("signal_bands expected 2 argument(s), got {}", args.len())));
    }
    let mut iter = args.into_iter();
    let count = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let phase = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let result = cargo_node_weave::signal_bands(count, phase);
    Ok(ToKainValue::to_kain_value(result))
}

fn __kain_bridge_beacon_pairs(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 3 {
        return Err(KainError::runtime(format!("beacon_pairs expected 3 argument(s), got {}", args.len())));
    }
    let mut iter = args.into_iter();
    let width = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let height = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let phase = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let result = cargo_node_weave::beacon_pairs(width, height, phase);
    Ok(ToKainValue::to_kain_value(result))
}

fn __kain_bridge_bar_energy(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 1 {
        return Err(KainError::runtime(format!("bar_energy expected 1 argument(s), got {}", args.len())));
    }
    let mut iter = args.into_iter();
    let values = <Vec<i64> as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let result = cargo_node_weave::bar_energy(values);
    Ok(ToKainValue::to_kain_value(result))
}

fn __kain_bridge_workbench_signature(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 2 {
        return Err(KainError::runtime(format!("workbench_signature expected 2 argument(s), got {}", args.len())));
    }
    let mut iter = args.into_iter();
    let __label_owned = <String as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let label = __label_owned.as_str();
    let phase = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let result = cargo_node_weave::workbench_signature(label, phase);
    Ok(ToKainValue::to_kain_value(result))
}

fn __kain_bridge_workbench_stamp_orbit(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 2 {
        return Err(KainError::runtime(format!("workbench_stamp_orbit expected 2 argument(s), got {}", args.len())));
    }
    let mut iter = args.into_iter();
    let seed = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let stride = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let result = cargo_node_weave::WorkbenchStamp::orbit(seed, stride);
    Ok(ToKainValue::to_kain_value(result))
}

fn register_all(env: &mut Env) {
    env.register_native_fn("signal_bands", __kain_bridge_signal_bands);
    env.register_native_fn("rust_cargo_node_weave_signal_bands", __kain_bridge_signal_bands);
    env.register_native_fn("beacon_pairs", __kain_bridge_beacon_pairs);
    env.register_native_fn("rust_cargo_node_weave_beacon_pairs", __kain_bridge_beacon_pairs);
    env.register_native_fn("bar_energy", __kain_bridge_bar_energy);
    env.register_native_fn("rust_cargo_node_weave_bar_energy", __kain_bridge_bar_energy);
    env.register_native_fn("workbench_signature", __kain_bridge_workbench_signature);
    env.register_native_fn("rust_cargo_node_weave_workbench_signature", __kain_bridge_workbench_signature);
    env.register_native_fn("workbench_stamp_orbit", __kain_bridge_workbench_stamp_orbit);
    env.register_native_fn("rust_cargo_node_weave_workbench_stamp_orbit", __kain_bridge_workbench_stamp_orbit);
}

#[no_mangle]
pub extern "C" fn kain_register_bridge(env: *mut Env) {
    let Some(env) = (unsafe { env.as_mut() }) else {
        return;
    };
    register_all(env);
}
