use kain_core::error::KainError;
use kain_core::runtime::{Env, Value};
use kain_host::{FromKainValue, ToKainValue};

fn __kain_bridge_add(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 2 {
        return Err(KainError::runtime(format!("add expected 2 argument(s), got {}", args.len())));
    }
    let mut iter = args.into_iter();
    let lhs = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let rhs = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let result = cargo_smoke_lab::add(lhs, rhs);
    Ok(ToKainValue::to_kain_value(result))
}

fn __kain_bridge_amplify_series(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 2 {
        return Err(KainError::runtime(format!("amplify_series expected 2 argument(s), got {}", args.len())));
    }
    let mut iter = args.into_iter();
    let values = <Vec<i64> as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let gain = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let result = cargo_smoke_lab::amplify_series(values, gain);
    Ok(ToKainValue::to_kain_value(result))
}

fn __kain_bridge_sum_series(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 1 {
        return Err(KainError::runtime(format!("sum_series expected 1 argument(s), got {}", args.len())));
    }
    let mut iter = args.into_iter();
    let values = <Vec<i64> as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let result = cargo_smoke_lab::sum_series(values);
    Ok(ToKainValue::to_kain_value(result))
}

fn __kain_bridge_every_above(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 2 {
        return Err(KainError::runtime(format!("every_above expected 2 argument(s), got {}", args.len())));
    }
    let mut iter = args.into_iter();
    let values = <Vec<i64> as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let floor = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let result = cargo_smoke_lab::every_above(values, floor);
    Ok(ToKainValue::to_kain_value(result))
}

fn __kain_bridge_compose_badge(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 2 {
        return Err(KainError::runtime(format!("compose_badge expected 2 argument(s), got {}", args.len())));
    }
    let mut iter = args.into_iter();
    let __label_owned = <String as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let label = __label_owned.as_str();
    let index = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let result = cargo_smoke_lab::compose_badge(label, index);
    Ok(ToKainValue::to_kain_value(result))
}

fn __kain_bridge_wave_row(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 2 {
        return Err(KainError::runtime(format!("wave_row expected 2 argument(s), got {}", args.len())));
    }
    let mut iter = args.into_iter();
    let width = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let phase = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let result = cargo_smoke_lab::wave_row(width, phase);
    Ok(ToKainValue::to_kain_value(result))
}

fn __kain_bridge_pulse_profile_stamp(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 2 {
        return Err(KainError::runtime(format!("pulse_profile_stamp expected 2 argument(s), got {}", args.len())));
    }
    let mut iter = args.into_iter();
    let seed = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let lanes = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let result = cargo_smoke_lab::PulseProfile::stamp(seed, lanes);
    Ok(ToKainValue::to_kain_value(result))
}

fn register_all(env: &mut Env) {
    env.register_native_fn("add", __kain_bridge_add);
    env.register_native_fn("rust_cargo_smoke_lab_add", __kain_bridge_add);
    env.register_native_fn("amplify_series", __kain_bridge_amplify_series);
    env.register_native_fn("rust_cargo_smoke_lab_amplify_series", __kain_bridge_amplify_series);
    env.register_native_fn("sum_series", __kain_bridge_sum_series);
    env.register_native_fn("rust_cargo_smoke_lab_sum_series", __kain_bridge_sum_series);
    env.register_native_fn("every_above", __kain_bridge_every_above);
    env.register_native_fn("rust_cargo_smoke_lab_every_above", __kain_bridge_every_above);
    env.register_native_fn("compose_badge", __kain_bridge_compose_badge);
    env.register_native_fn("rust_cargo_smoke_lab_compose_badge", __kain_bridge_compose_badge);
    env.register_native_fn("wave_row", __kain_bridge_wave_row);
    env.register_native_fn("rust_cargo_smoke_lab_wave_row", __kain_bridge_wave_row);
    env.register_native_fn("pulse_profile_stamp", __kain_bridge_pulse_profile_stamp);
    env.register_native_fn("rust_cargo_smoke_lab_pulse_profile_stamp", __kain_bridge_pulse_profile_stamp);
}

#[no_mangle]
pub extern "C" fn kain_register_bridge(env: *mut Env) {
    let Some(env) = (unsafe { env.as_mut() }) else {
        return;
    };
    register_all(env);
}
