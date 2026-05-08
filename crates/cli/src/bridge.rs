use std::fs;
use std::io::{self, BufRead, Write};
use std::path::PathBuf;

use kain_core::runtime::{Env, Value};
use kain_core::CompileTarget;
use serde::Serialize;
use serde_json::{json, Map, Value as JsonValue};

#[derive(Debug, Clone)]
pub struct BridgeServeConfig {
    pub entry: PathBuf,
    pub dispatch_function: String,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct BridgeResponse {
    id: JsonValue,
    ok: bool,
    #[serde(skip_serializing_if = "Option::is_none")]
    result: Option<JsonValue>,
    #[serde(skip_serializing_if = "Option::is_none")]
    error: Option<String>,
}

pub fn run_bridge_server(config: BridgeServeConfig) -> Result<(), String> {
    let entry = config
        .entry
        .canonicalize()
        .map_err(|error| format!("Failed to resolve Kain bridge entry: {error}"))?;
    let source = fs::read_to_string(&entry).map_err(|error| {
        format!(
            "Failed to read Kain bridge entry {}: {error}",
            entry.display()
        )
    })?;

    if let Some(parent) = entry.parent() {
        std::env::set_current_dir(parent).map_err(|error| {
            format!(
                "Failed to set Kain bridge cwd to {}: {error}",
                parent.display()
            )
        })?;
    }

    let typed_program = kain_driver::DriverSession::default()
        .frontend_to_typed_program(&source, CompileTarget::Interpret)
        .map_err(|error| {
            format!(
                "Failed to compile Kain bridge entry {}: {error}",
                entry.display()
            )
        })?;

    let mut env = Env::new();
    env.register_typed_program(&typed_program)
        .map_err(|error| {
            format!(
                "Failed to load Kain bridge entry {}: {error}",
                entry.display()
            )
        })?;

    let stdin = io::stdin();
    let mut stdout = io::stdout();

    for line in stdin.lock().lines() {
        let line = match line {
            Ok(line) => line,
            Err(error) => {
                write_response(
                    &mut stdout,
                    BridgeResponse {
                        id: JsonValue::Null,
                        ok: false,
                        result: None,
                        error: Some(format!("Failed to read Kain bridge request: {error}")),
                    },
                )?;
                continue;
            }
        };

        if line.trim().is_empty() {
            continue;
        }

        let request: JsonValue = match serde_json::from_str(&line) {
            Ok(value) => value,
            Err(error) => {
                write_response(
                    &mut stdout,
                    BridgeResponse {
                        id: JsonValue::Null,
                        ok: false,
                        result: None,
                        error: Some(format!("Failed to parse Kain bridge request JSON: {error}")),
                    },
                )?;
                continue;
            }
        };
        let id = request.get("id").cloned().unwrap_or(JsonValue::Null);

        let result = env.call_named_function(
            &config.dispatch_function,
            vec![Value::String(request.to_string())],
        );

        match result {
            Ok(value) => write_response(
                &mut stdout,
                BridgeResponse {
                    id,
                    ok: true,
                    result: Some(value_to_json(&value)),
                    error: None,
                },
            )?,
            Err(error) => write_response(
                &mut stdout,
                BridgeResponse {
                    id,
                    ok: false,
                    result: None,
                    error: Some(error.to_string()),
                },
            )?,
        }
    }

    Ok(())
}

fn write_response(stdout: &mut io::Stdout, response: BridgeResponse) -> Result<(), String> {
    let line = serde_json::to_string(&response)
        .map_err(|error| format!("Failed to serialize Kain bridge response: {error}"))?;
    stdout
        .write_all(line.as_bytes())
        .and_then(|_| stdout.write_all(b"\n"))
        .and_then(|_| stdout.flush())
        .map_err(|error| format!("Failed to write Kain bridge response: {error}"))
}

fn value_to_json(value: &Value) -> JsonValue {
    match value {
        Value::Unit | Value::None => JsonValue::Null,
        Value::Bool(value) => JsonValue::Bool(*value),
        Value::Int(value) => json!(value),
        Value::Float(value) => {
            serde_json::Number::from_f64(*value).map_or(JsonValue::Null, JsonValue::Number)
        }
        Value::String(value) => JsonValue::String(value.clone()),
        Value::Array(values) => JsonValue::Array(
            values
                .read()
                .map(|values| values.iter().map(value_to_json).collect())
                .unwrap_or_default(),
        ),
        Value::Tuple(values) => JsonValue::Array(values.iter().map(value_to_json).collect()),
        Value::Struct(_, fields) => {
            let mut object = Map::new();
            if let Ok(fields) = fields.read() {
                for (key, value) in fields.iter() {
                    object.insert(key.clone(), value_to_json(value));
                }
            }
            JsonValue::Object(object)
        }
        Value::Result(ok, value) => json!({
            "ok": ok,
            "value": value_to_json(value),
        }),
        Value::EnumVariant(enum_name, variant_name, fields) => json!({
            "enum": enum_name,
            "variant": variant_name,
            "fields": fields.iter().map(value_to_json).collect::<Vec<_>>(),
        }),
        Value::Return(value) => value_to_json(value),
        Value::Break(value) => json!({
            "control": "break",
            "value": value.as_deref().map(value_to_json),
        }),
        Value::Continue => json!({ "control": "continue" }),
        Value::Poll(ready, value) => json!({
            "ready": ready,
            "value": value.as_deref().map(value_to_json),
        }),
        _ => JsonValue::String(value.to_string()),
    }
}
