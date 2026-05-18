use std::path::PathBuf;

use kain_run::{
    execute_run, plan_run, render_text_report, RunMode, RunReport, RunRequest, RunStatus, RunTarget,
};

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
    let report = execute_run(&request).map_err(|err| err.to_string())?;
    print_report(&report, request.json)?;
    if report.status == RunStatus::Failed {
        Err(format!(
            "run failed; report written to {}",
            report.report_path.display()
        ))
    } else {
        Ok(())
    }
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

trait Tap: Sized {
    fn tap(mut self, f: impl FnOnce(&mut Self)) -> Self {
        f(&mut self);
        self
    }
}

impl<T> Tap for T {}
