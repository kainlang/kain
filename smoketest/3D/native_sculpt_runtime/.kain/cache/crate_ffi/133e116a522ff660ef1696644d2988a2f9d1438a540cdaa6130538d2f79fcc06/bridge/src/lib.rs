use kain_core::error::KainError;
use kain_core::runtime::{Env, Value};
use kain_host::{FromKainValue, ToKainValue};

fn __kain_bridge_native_sculpt_curve(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 4 {
        return Err(KainError::runtime(format!("native_sculpt_curve expected 4 argument(s), got {}", args.len())));
    }
    let mut iter = args.into_iter();
    let radius = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let intensity = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let hardness = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let samples = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let result = native_sculpt_backend::native_sculpt_curve(radius, intensity, hardness, samples);
    Ok(ToKainValue::to_kain_value(result))
}

fn __kain_bridge_native_sculpt_forecast(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 4 {
        return Err(KainError::runtime(format!("native_sculpt_forecast expected 4 argument(s), got {}", args.len())));
    }
    let mut iter = args.into_iter();
    let base_polys = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let dyn_topo = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let detail_pct = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let layers = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let result = native_sculpt_backend::native_sculpt_forecast(base_polys, dyn_topo, detail_pct, layers);
    Ok(ToKainValue::to_kain_value(result))
}

fn __kain_bridge_native_sculpt_stroke_energy(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 4 {
        return Err(KainError::runtime(format!("native_sculpt_stroke_energy expected 4 argument(s), got {}", args.len())));
    }
    let mut iter = args.into_iter();
    let radius = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let intensity = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let spacing = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let accumulate = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let result = native_sculpt_backend::native_sculpt_stroke_energy(radius, intensity, spacing, accumulate);
    Ok(ToKainValue::to_kain_value(result))
}

fn __kain_bridge_native_sculpt_runtime_signature(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 3 {
        return Err(KainError::runtime(format!("native_sculpt_runtime_signature expected 3 argument(s), got {}", args.len())));
    }
    let mut iter = args.into_iter();
    let tool = <String as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let energy = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let target_polys = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let result = native_sculpt_backend::native_sculpt_runtime_signature(tool, energy, target_polys);
    Ok(ToKainValue::to_kain_value(result))
}

fn register_all(env: &mut Env) {
    env.register_native_fn("native_sculpt_curve", __kain_bridge_native_sculpt_curve);
    env.register_native_fn("rust_native_sculpt_backend_native_sculpt_curve", __kain_bridge_native_sculpt_curve);
    env.register_native_fn("native_sculpt_forecast", __kain_bridge_native_sculpt_forecast);
    env.register_native_fn("rust_native_sculpt_backend_native_sculpt_forecast", __kain_bridge_native_sculpt_forecast);
    env.register_native_fn("native_sculpt_stroke_energy", __kain_bridge_native_sculpt_stroke_energy);
    env.register_native_fn("rust_native_sculpt_backend_native_sculpt_stroke_energy", __kain_bridge_native_sculpt_stroke_energy);
    env.register_native_fn("native_sculpt_runtime_signature", __kain_bridge_native_sculpt_runtime_signature);
    env.register_native_fn("rust_native_sculpt_backend_native_sculpt_runtime_signature", __kain_bridge_native_sculpt_runtime_signature);
}

#[no_mangle]
pub extern "C" fn kain_register_bridge(env: *mut Env) {
    let Some(env) = (unsafe { env.as_mut() }) else {
        return;
    };
    register_all(env);
}
