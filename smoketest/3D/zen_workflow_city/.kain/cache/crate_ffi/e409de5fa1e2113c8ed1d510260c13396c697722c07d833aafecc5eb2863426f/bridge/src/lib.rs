use kain_core::error::KainError;
use kain_core::runtime::{Env, Value};
use kain_host::{FromKainValue, ToKainValue};

fn __kain_bridge_workflow_group_labels(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 0 {
        return Err(KainError::runtime(format!("workflow_group_labels expected 0 argument(s), got {}", args.len())));
    }
    let mut iter = args.into_iter();
    let result = workflow_city_lab::workflow_group_labels();
    Ok(ToKainValue::to_kain_value(result))
}

fn __kain_bridge_workflow_group_colors(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 0 {
        return Err(KainError::runtime(format!("workflow_group_colors expected 0 argument(s), got {}", args.len())));
    }
    let mut iter = args.into_iter();
    let result = workflow_city_lab::workflow_group_colors();
    Ok(ToKainValue::to_kain_value(result))
}

fn __kain_bridge_workflow_group_anchor_xs(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 0 {
        return Err(KainError::runtime(format!("workflow_group_anchor_xs expected 0 argument(s), got {}", args.len())));
    }
    let mut iter = args.into_iter();
    let result = workflow_city_lab::workflow_group_anchor_xs();
    Ok(ToKainValue::to_kain_value(result))
}

fn __kain_bridge_workflow_group_anchor_zs(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 0 {
        return Err(KainError::runtime(format!("workflow_group_anchor_zs expected 0 argument(s), got {}", args.len())));
    }
    let mut iter = args.into_iter();
    let result = workflow_city_lab::workflow_group_anchor_zs();
    Ok(ToKainValue::to_kain_value(result))
}

fn __kain_bridge_workflow_group_spans(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 0 {
        return Err(KainError::runtime(format!("workflow_group_spans expected 0 argument(s), got {}", args.len())));
    }
    let mut iter = args.into_iter();
    let result = workflow_city_lab::workflow_group_spans();
    Ok(ToKainValue::to_kain_value(result))
}

fn __kain_bridge_workflow_group_module_counts(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 0 {
        return Err(KainError::runtime(format!("workflow_group_module_counts expected 0 argument(s), got {}", args.len())));
    }
    let mut iter = args.into_iter();
    let result = workflow_city_lab::workflow_group_module_counts();
    Ok(ToKainValue::to_kain_value(result))
}

fn __kain_bridge_workflow_module_names(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 0 {
        return Err(KainError::runtime(format!("workflow_module_names expected 0 argument(s), got {}", args.len())));
    }
    let mut iter = args.into_iter();
    let result = workflow_city_lab::workflow_module_names();
    Ok(ToKainValue::to_kain_value(result))
}

fn __kain_bridge_workflow_module_group_indices(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 0 {
        return Err(KainError::runtime(format!("workflow_module_group_indices expected 0 argument(s), got {}", args.len())));
    }
    let mut iter = args.into_iter();
    let result = workflow_city_lab::workflow_module_group_indices();
    Ok(ToKainValue::to_kain_value(result))
}

fn __kain_bridge_workflow_module_xs(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 0 {
        return Err(KainError::runtime(format!("workflow_module_xs expected 0 argument(s), got {}", args.len())));
    }
    let mut iter = args.into_iter();
    let result = workflow_city_lab::workflow_module_xs();
    Ok(ToKainValue::to_kain_value(result))
}

fn __kain_bridge_workflow_module_zs(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 0 {
        return Err(KainError::runtime(format!("workflow_module_zs expected 0 argument(s), got {}", args.len())));
    }
    let mut iter = args.into_iter();
    let result = workflow_city_lab::workflow_module_zs();
    Ok(ToKainValue::to_kain_value(result))
}

fn __kain_bridge_workflow_module_heights(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 0 {
        return Err(KainError::runtime(format!("workflow_module_heights expected 0 argument(s), got {}", args.len())));
    }
    let mut iter = args.into_iter();
    let result = workflow_city_lab::workflow_module_heights();
    Ok(ToKainValue::to_kain_value(result))
}

fn __kain_bridge_workflow_module_energies(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 0 {
        return Err(KainError::runtime(format!("workflow_module_energies expected 0 argument(s), got {}", args.len())));
    }
    let mut iter = args.into_iter();
    let result = workflow_city_lab::workflow_module_energies();
    Ok(ToKainValue::to_kain_value(result))
}

fn __kain_bridge_workflow_total_energy(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 0 {
        return Err(KainError::runtime(format!("workflow_total_energy expected 0 argument(s), got {}", args.len())));
    }
    let mut iter = args.into_iter();
    let result = workflow_city_lab::workflow_total_energy();
    Ok(ToKainValue::to_kain_value(result))
}

fn __kain_bridge_workflow_city_signature(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {
    if args.len() != 1 {
        return Err(KainError::runtime(format!("workflow_city_signature expected 1 argument(s), got {}", args.len())));
    }
    let mut iter = args.into_iter();
    let seed = <i64 as FromKainValue>::from_kain_value(iter.next().expect("checked arg count"))?;
    let result = workflow_city_lab::workflow_city_signature(seed);
    Ok(ToKainValue::to_kain_value(result))
}

fn register_all(env: &mut Env) {
    env.register_native_fn("workflow_group_labels", __kain_bridge_workflow_group_labels);
    env.register_native_fn("rust_workflow_city_lab_workflow_group_labels", __kain_bridge_workflow_group_labels);
    env.register_native_fn("workflow_group_colors", __kain_bridge_workflow_group_colors);
    env.register_native_fn("rust_workflow_city_lab_workflow_group_colors", __kain_bridge_workflow_group_colors);
    env.register_native_fn("workflow_group_anchor_xs", __kain_bridge_workflow_group_anchor_xs);
    env.register_native_fn("rust_workflow_city_lab_workflow_group_anchor_xs", __kain_bridge_workflow_group_anchor_xs);
    env.register_native_fn("workflow_group_anchor_zs", __kain_bridge_workflow_group_anchor_zs);
    env.register_native_fn("rust_workflow_city_lab_workflow_group_anchor_zs", __kain_bridge_workflow_group_anchor_zs);
    env.register_native_fn("workflow_group_spans", __kain_bridge_workflow_group_spans);
    env.register_native_fn("rust_workflow_city_lab_workflow_group_spans", __kain_bridge_workflow_group_spans);
    env.register_native_fn("workflow_group_module_counts", __kain_bridge_workflow_group_module_counts);
    env.register_native_fn("rust_workflow_city_lab_workflow_group_module_counts", __kain_bridge_workflow_group_module_counts);
    env.register_native_fn("workflow_module_names", __kain_bridge_workflow_module_names);
    env.register_native_fn("rust_workflow_city_lab_workflow_module_names", __kain_bridge_workflow_module_names);
    env.register_native_fn("workflow_module_group_indices", __kain_bridge_workflow_module_group_indices);
    env.register_native_fn("rust_workflow_city_lab_workflow_module_group_indices", __kain_bridge_workflow_module_group_indices);
    env.register_native_fn("workflow_module_xs", __kain_bridge_workflow_module_xs);
    env.register_native_fn("rust_workflow_city_lab_workflow_module_xs", __kain_bridge_workflow_module_xs);
    env.register_native_fn("workflow_module_zs", __kain_bridge_workflow_module_zs);
    env.register_native_fn("rust_workflow_city_lab_workflow_module_zs", __kain_bridge_workflow_module_zs);
    env.register_native_fn("workflow_module_heights", __kain_bridge_workflow_module_heights);
    env.register_native_fn("rust_workflow_city_lab_workflow_module_heights", __kain_bridge_workflow_module_heights);
    env.register_native_fn("workflow_module_energies", __kain_bridge_workflow_module_energies);
    env.register_native_fn("rust_workflow_city_lab_workflow_module_energies", __kain_bridge_workflow_module_energies);
    env.register_native_fn("workflow_total_energy", __kain_bridge_workflow_total_energy);
    env.register_native_fn("rust_workflow_city_lab_workflow_total_energy", __kain_bridge_workflow_total_energy);
    env.register_native_fn("workflow_city_signature", __kain_bridge_workflow_city_signature);
    env.register_native_fn("rust_workflow_city_lab_workflow_city_signature", __kain_bridge_workflow_city_signature);
}

#[no_mangle]
pub extern "C" fn kain_register_bridge(env: *mut Env) {
    let Some(env) = (unsafe { env.as_mut() }) else {
        return;
    };
    register_all(env);
}
