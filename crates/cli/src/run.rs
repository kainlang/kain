use std::fs;
use std::path::{Path, PathBuf};

use kain_driver::discover_native_app_root_component;
use serde_json::json;
use kain_run::{
    execute_run, plan_run, render_text_report, RunMode, RunReport, RunRequest, RunStatus, RunTarget,
};

use crate::native_ui_build::{NativeUiBuildConfig, NativeUiRuntimeDependencyConfig};

pub fn make_request(
    input: Option<PathBuf>,
    mode: RunMode,
    target: String,
    args: Vec<String>,
    json: bool,
    trace: bool,
    keep_artifacts: bool,
    dry_run: bool,
) -> Result<RunRequest, String> {
    let input = match input {
        Some(path) => match crate::amalgamate::maybe_materialize_input(&path)? {
            Some(materialized) => Some(
                materialized
                    .runnable_input()
                    .map_err(|err| err.to_string())?,
            ),
            None => Some(path),
        },
        None => None,
    };
    let target = RunTarget::parse(&target).map_err(|err| err.to_string())?;
    Ok(RunRequest::new(input)
        .with_mode(mode)
        .with_target(target)
        .with_args(args)
        .with_workspace_path(PathBuf::from("."))
        .with_blade(None)
        .tap(|request| {
            request.json = json;
            request.trace = trace;
            request.keep_artifacts = keep_artifacts;
            request.dry_run = dry_run;
        }))
}

pub fn make_blade_request(
    blade: Option<String>,
    path: PathBuf,
    target: String,
    args: Vec<String>,
    json: bool,
    trace: bool,
    keep_artifacts: bool,
    dry_run: bool,
) -> Result<RunRequest, String> {
    let target = RunTarget::parse(&target).map_err(|err| err.to_string())?;
    Ok(RunRequest::new(None)
        .with_mode(RunMode::Once)
        .with_target(target)
        .with_args(args)
        .with_workspace_path(path)
        .with_blade(blade)
        .tap(|request| {
            request.json = json;
            request.trace = trace;
            request.keep_artifacts = keep_artifacts;
            request.dry_run = dry_run;
        }))
}

pub fn execute(request: RunRequest) -> Result<(), String> {
    if request.mode == RunMode::Plan {
        return print_plan(request);
    }
    if let Some(config) = native_ui_dev_config_for_request(&request)? {
        return crate::native_ui_dev::run_native_ui_dev(config).map_err(|err| err.to_string());
    }
    let report = execute_run(&request).map_err(|err| {
        let rendered_output = format!(" Run failed: {err}\n");
        let _ = kain_core::diagnostic_capture::capture_event_if_enabled(
            kain_core::diagnostic_capture::CapturedDiagnosticEventInput {
                event_kind: "run-execute-failure".to_string(),
                command: if request.mode == RunMode::Dev {
                    "watch".to_string()
                } else {
                    "run".to_string()
                },
                argv: std::env::args().collect(),
                cwd: std::env::current_dir()
                    .unwrap_or_else(|_| PathBuf::from("."))
                    .display()
                    .to_string(),
                launcher: None,
                target: Some(format!("{:?}", request.target).to_ascii_lowercase()),
                source_name: request
                    .input
                    .as_ref()
                    .and_then(|path| path.file_name())
                    .and_then(|value| value.to_str())
                    .map(str::to_string),
                source_path: request
                    .input
                    .as_ref()
                    .map(|path| path.display().to_string()),
                rendered_output,
                structured_diagnostic: None,
                tags: Vec::new(),
                context: json!({
                    "requested_mode": format!("{:?}", request.mode),
                    "requested_target": format!("{:?}", request.target),
                    "error_display": err.to_string(),
                }),
            },
        );
        err.to_string()
    })?;
    print_report(&report, request.json)?;
    if report.status == RunStatus::Failed {
        capture_run_failure(&request, &report);
        Err(format!(
            "run failed; report written to {}",
            report.report_path.display()
        ))
    } else {
        Ok(())
    }
}

fn capture_run_failure(request: &RunRequest, report: &RunReport) {
    let rendered_output = kain_run::render_compact_output(report);
    if rendered_output.trim().is_empty() {
        return;
    }
    let _ = kain_core::diagnostic_capture::capture_event_if_enabled(
        kain_core::diagnostic_capture::CapturedDiagnosticEventInput {
            event_kind: "run-report-failure".to_string(),
            command: if request.mode == RunMode::Dev {
                "watch".to_string()
            } else {
                "run".to_string()
            },
            argv: std::env::args().collect(),
            cwd: std::env::current_dir()
                .unwrap_or_else(|_| PathBuf::from("."))
                .display()
                .to_string(),
            launcher: None,
            target: Some(format!("{:?}", request.target).to_ascii_lowercase()),
            source_name: request
                .input
                .as_ref()
                .and_then(|path| path.file_name())
                .and_then(|value| value.to_str())
                .map(str::to_string),
            source_path: request
                .input
                .as_ref()
                .map(|path| path.display().to_string()),
            rendered_output,
            structured_diagnostic: None,
            tags: Vec::new(),
            context: json!({
                "requested_mode": format!("{:?}", request.mode),
                "requested_target": format!("{:?}", request.target),
                "report_path": report.report_path.display().to_string(),
                "events_path": report.events_path.display().to_string(),
                "workspace_root": report.workspace_root.display().to_string(),
                "status": format!("{:?}", report.status),
                "units": report.units.iter().map(|unit| json!({
                    "id": &unit.id,
                    "target": format!("{:?}", unit.target),
                    "status": format!("{:?}", unit.status),
                    "exit_code": unit.exit_code,
                    "error": &unit.error,
                })).collect::<Vec<_>>(),
            }),
        },
    );
}

pub fn print_plan(request: RunRequest) -> Result<(), String> {
    let plan = plan_run(&request).map_err(|err| err.to_string())?;
    if request.json {
        print_json(&plan)
    } else {
        println!(
            "Run plan: {:?} target={:?} workspace={}",
            plan.mode,
            plan.requested_target,
            plan.workspace_root.display()
        );
        for unit in &plan.units {
            println!("  {} {:?} cwd={}", unit.id, unit.target, unit.cwd.display());
            for input in &unit.inputs {
                println!("    input {}", input.display());
            }
        }
        println!("Report root: {}", plan.report_root.display());
        Ok(())
    }
}

pub fn print_report(report: &RunReport, json: bool) -> Result<(), String> {
    if json {
        print_json(report)
    } else {
        print!("{}", render_text_report(report));
        Ok(())
    }
}

fn print_json<T: serde::Serialize>(value: &T) -> Result<(), String> {
    let text = serde_json::to_string_pretty(value)
        .map_err(|err| format!("failed to serialize run JSON: {err}"))?;
    println!("{text}");
    Ok(())
}

fn native_ui_dev_config_for_request(
    request: &RunRequest,
) -> Result<Option<crate::native_ui_dev::NativeUiDevConfig>, String> {
    if request.mode != RunMode::Dev
        || request.json
        || request.dry_run
        || !request.args.is_empty()
        || request.blade.is_some()
        || !matches!(request.target, RunTarget::Auto | RunTarget::Kain)
    {
        return Ok(None);
    }
    let Some(input) = request.input.as_ref() else {
        return Ok(None);
    };
    if !is_kain_source_input(input) {
        return Ok(None);
    }

    let source = fs::read_to_string(input)
        .map_err(|err| format!("failed to read {}: {err}", input.display()))?;
    let source_name = input
        .file_name()
        .and_then(|value| value.to_str())
        .unwrap_or("app.kn");
    let Some(_) = discover_native_app_root_component(&source, None, source_name)
        .map_err(|err| err.to_string())?
    else {
        return Ok(None);
    };

    let build = NativeUiBuildConfig {
        build_executable: true,
        runtime_dependency: NativeUiRuntimeDependencyConfig::WorkspacePath,
        ..Default::default()
    };
    crate::native_ui_dev::NativeUiDevConfig::new(input.clone(), build)
        .map(Some)
        .map_err(|err| err.to_string())
}

fn is_kain_source_input(path: &Path) -> bool {
    path.extension()
        .and_then(|value| value.to_str())
        .is_some_and(|ext| ext.eq_ignore_ascii_case("kn"))
}

trait Tap: Sized {
    fn tap(mut self, f: impl FnOnce(&mut Self)) -> Self {
        f(&mut self);
        self
    }
}

impl<T> Tap for T {}

#[cfg(test)]
mod tests {
    use super::*;
    use tempfile::TempDir;

    fn dev_request(path: PathBuf) -> RunRequest {
        RunRequest::new(Some(path))
            .with_mode(RunMode::Dev)
            .with_target(RunTarget::Auto)
    }

    #[test]
    fn native_ui_dev_config_detects_component_apps() {
        let temp = TempDir::new().expect("temp dir");
        let input = temp.path().join("app.kn");
        fs::write(
            &input,
            r#"
component App():
    render <panel title="Reload Test" />
"#,
        )
        .expect("write component app");

        let config = native_ui_dev_config_for_request(&dev_request(input.clone()))
            .expect("native ui dev detection")
            .expect("component app should route to native ui dev");
        assert_eq!(config.input, input);
    }

    #[test]
    fn native_ui_dev_config_skips_non_ui_sources() {
        let temp = TempDir::new().expect("temp dir");
        let input = temp.path().join("main.kn");
        fs::write(
            &input,
            r#"
fn main() -> Int:
    return 0
"#,
        )
        .expect("write non-ui source");

        let config =
            native_ui_dev_config_for_request(&dev_request(input)).expect("native ui dev detection");
        assert!(config.is_none());
    }
}
