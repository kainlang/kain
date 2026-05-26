use crate::tooling_config::{
    active_kain_tooling_config, KainDiagnosticCaptureMode, ResolvedKainDiagnosticsConfig,
};
use serde::Serialize;
use serde_json::Value as JsonValue;
use std::collections::hash_map::DefaultHasher;
use std::fs::{self, OpenOptions};
use std::hash::{Hash, Hasher};
use std::io::Write;

const CAPTURE_SCHEMA_VERSION: u32 = 1;

#[derive(Debug, Clone, Serialize)]
pub struct CapturedDiagnosticEvent {
    pub schema_version: u32,
    pub captured_at_unix_ms: u128,
    pub process_id: u32,
    pub fingerprint: String,
    pub event_kind: String,
    pub command: String,
    pub argv: Vec<String>,
    pub cwd: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub launcher: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub target: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub source_name: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub source_path: Option<String>,
    pub rendered_plain_text: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub rendered_ansi: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub structured_diagnostic: Option<JsonValue>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub tags: Vec<String>,
    pub context: JsonValue,
}

#[derive(Debug, Clone)]
pub struct CapturedDiagnosticEventInput {
    pub event_kind: String,
    pub command: String,
    pub argv: Vec<String>,
    pub cwd: String,
    pub launcher: Option<String>,
    pub target: Option<String>,
    pub source_name: Option<String>,
    pub source_path: Option<String>,
    pub rendered_output: String,
    pub structured_diagnostic: Option<JsonValue>,
    pub tags: Vec<String>,
    pub context: JsonValue,
}

impl CapturedDiagnosticEventInput {
    pub fn into_event(self, config: &ResolvedKainDiagnosticsConfig) -> CapturedDiagnosticEvent {
        let rendered_plain_text = strip_ansi_sequences(&self.rendered_output);
        let rendered_ansi = if config.store_ansi {
            Some(self.rendered_output)
        } else {
            None
        };
        CapturedDiagnosticEvent {
            schema_version: CAPTURE_SCHEMA_VERSION,
            captured_at_unix_ms: now_unix_ms(),
            process_id: std::process::id(),
            fingerprint: fingerprint_for_event(
                &self.event_kind,
                &self.command,
                self.source_name.as_deref(),
                self.source_path.as_deref(),
                &rendered_plain_text,
            ),
            event_kind: self.event_kind,
            command: self.command,
            argv: self.argv,
            cwd: self.cwd,
            launcher: self.launcher,
            target: self.target,
            source_name: self.source_name,
            source_path: self.source_path,
            rendered_plain_text,
            rendered_ansi,
            structured_diagnostic: self.structured_diagnostic,
            tags: self.tags,
            context: self.context,
        }
    }
}

pub fn capture_event_if_enabled(input: CapturedDiagnosticEventInput) -> Result<bool, String> {
    let config = active_kain_tooling_config().diagnostics;
    if !matches!(config.capture, KainDiagnosticCaptureMode::Failures) {
        return Ok(false);
    }
    append_event(&config, input.into_event(&config))?;
    Ok(true)
}

pub fn strip_ansi_sequences(input: &str) -> String {
    let mut output = String::with_capacity(input.len());
    let mut chars = input.chars().peekable();

    while let Some(ch) = chars.next() {
        if ch != '\u{1b}' {
            output.push(ch);
            continue;
        }

        if !matches!(chars.peek(), Some('[')) {
            continue;
        }

        chars.next();
        while let Some(next) = chars.next() {
            if ('@'..='~').contains(&next) {
                break;
            }
        }
    }

    output
}

fn append_event(
    config: &ResolvedKainDiagnosticsConfig,
    event: CapturedDiagnosticEvent,
) -> Result<(), String> {
    if let Some(parent) = config.path.parent() {
        fs::create_dir_all(parent).map_err(|err| {
            format!(
                "failed to create diagnostics capture directory {}: {err}",
                parent.display()
            )
        })?;
    }

    let encoded = serde_json::to_string(&event)
        .map_err(|err| format!("failed to serialize diagnostics capture event: {err}"))?;
    let mut file = OpenOptions::new()
        .create(true)
        .append(true)
        .open(&config.path)
        .map_err(|err| {
            format!(
                "failed to open diagnostics capture log {}: {err}",
                config.path.display()
            )
        })?;
    writeln!(file, "{encoded}").map_err(|err| {
        format!(
            "failed to append diagnostics capture log {}: {err}",
            config.path.display()
        )
    })
}

fn fingerprint_for_event(
    event_kind: &str,
    command: &str,
    source_name: Option<&str>,
    source_path: Option<&str>,
    rendered_plain_text: &str,
) -> String {
    let mut hasher = DefaultHasher::new();
    event_kind.hash(&mut hasher);
    command.hash(&mut hasher);
    source_name.hash(&mut hasher);
    source_path.hash(&mut hasher);
    rendered_plain_text.hash(&mut hasher);
    format!("{:016x}", hasher.finish())
}

fn now_unix_ms() -> u128 {
    std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .map(|value| value.as_millis())
        .unwrap_or(0)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::tooling_config::{
        install_active_kain_tooling_config, KainDiagnosticCaptureMode, ResolvedKainToolingConfig,
    };
    use once_cell::sync::Lazy;
    use serde_json::json;
    use std::path::PathBuf;
    use std::sync::{Mutex, MutexGuard};

    static CAPTURE_TEST_LOCK: Lazy<Mutex<()>> = Lazy::new(|| Mutex::new(()));

    fn lock_capture_test() -> MutexGuard<'static, ()> {
        CAPTURE_TEST_LOCK
            .lock()
            .unwrap_or_else(|poisoned| poisoned.into_inner())
    }

    #[test]
    fn strip_ansi_sequences_removes_color_codes() {
        assert_eq!(
            strip_ansi_sequences("\u{1b}[31merror\u{1b}[0m: bad news"),
            "error: bad news"
        );
    }

    #[test]
    fn capture_event_appends_jsonl_when_enabled() {
        let _guard = lock_capture_test();
        let temp = tempfile::tempdir().expect("temp dir");
        let mut config = ResolvedKainToolingConfig::default();
        config.diagnostics.capture = KainDiagnosticCaptureMode::Failures;
        config.diagnostics.path = temp.path().join("errors.jsonl");
        config.diagnostics.store_ansi = true;
        install_active_kain_tooling_config(config);

        let captured = capture_event_if_enabled(CapturedDiagnosticEventInput {
            event_kind: "compile-failure".to_string(),
            command: "build".to_string(),
            argv: vec!["kain".to_string(), "build".to_string()],
            cwd: temp.path().display().to_string(),
            launcher: Some("kain".to_string()),
            target: Some("llvm".to_string()),
            source_name: Some("main.kn".to_string()),
            source_path: Some(temp.path().join("main.kn").display().to_string()),
            rendered_output: "\u{1b}[31merror[Test]\u{1b}[0m: boom".to_string(),
            structured_diagnostic: Some(json!({"code": "KAIN-PARSE-0001"})),
            tags: vec!["parse".to_string()],
            context: json!({"phase": "parse"}),
        })
        .expect("capture succeeds");

        assert!(captured);
        let written = fs::read_to_string(temp.path().join("errors.jsonl")).expect("capture file");
        assert!(written.contains("\"event_kind\":\"compile-failure\""));
        assert!(written.contains("\"rendered_plain_text\":\"error[Test]: boom\""));
        assert!(written.contains("\"rendered_ansi\":\"\\u001b[31merror[Test]\\u001b[0m: boom\""));
    }

    #[test]
    fn capture_event_skips_when_disabled() {
        let _guard = lock_capture_test();
        let temp = tempfile::tempdir().expect("temp dir");
        let mut config = ResolvedKainToolingConfig::default();
        config.diagnostics.capture = KainDiagnosticCaptureMode::Off;
        config.diagnostics.path = PathBuf::from(temp.path()).join("errors.jsonl");
        install_active_kain_tooling_config(config);

        let captured = capture_event_if_enabled(CapturedDiagnosticEventInput {
            event_kind: "compile-failure".to_string(),
            command: "build".to_string(),
            argv: vec![],
            cwd: ".".to_string(),
            launcher: None,
            target: None,
            source_name: None,
            source_path: None,
            rendered_output: "error: skipped".to_string(),
            structured_diagnostic: None,
            tags: Vec::new(),
            context: json!({}),
        })
        .expect("capture returns");

        assert!(!captured);
        assert!(!temp.path().join("errors.jsonl").exists());
    }
}
