use kain_driver::{
    CompilerProgressPhase, ToolingProgressEvent, ToolingProgressSink, ToolingProgressStatus,
};
use std::io::{self, IsTerminal};
use std::path::{Path, PathBuf};
use std::sync::{Arc, Mutex};

pub fn stderr_progress_sink(enabled: bool) -> Option<ToolingProgressSink> {
    if !enabled || !io::stderr().is_terminal() {
        return None;
    }
    let cwd = std::env::current_dir().unwrap_or_else(|_| PathBuf::from("."));
    let write_lock = Arc::new(Mutex::new(()));
    Some(ToolingProgressSink::new(move |event| {
        let Some(line) = render_event(event, &cwd) else {
            return;
        };
        if let Ok(_guard) = write_lock.lock() {
            eprintln!("{line}");
        }
    }))
}

fn render_event(event: &ToolingProgressEvent, cwd: &Path) -> Option<String> {
    match event {
        ToolingProgressEvent::CheckDiscoveryStarted { root, target } => Some(format!(
            " Discovering Kain sources under {} for {}...",
            display_path(root, cwd),
            target
        )),
        ToolingProgressEvent::CheckDiscoveryFinished {
            total_files,
            target,
            ..
        } => Some(format!(" Checking {total_files} file(s) for {target}...")),
        ToolingProgressEvent::CheckFileStarted {
            current,
            total,
            path,
            ..
        } => Some(format!(
            " Checking {current}/{total}: {}",
            display_path(path, cwd)
        )),
        ToolingProgressEvent::CheckFileFinished {
            current,
            total,
            path,
            status,
            ..
        } => match status {
            ToolingProgressStatus::Failed => Some(format!(
                " Check failed {current}/{total}: {}",
                display_path(path, cwd)
            )),
            _ => None,
        },
        ToolingProgressEvent::BuildPlanReady {
            total_tasks,
            target,
            workspace_root,
            ..
        } => Some(format!(
            " Planned {total_tasks} build task(s) for {target} in {}",
            display_path(workspace_root, cwd)
        )),
        ToolingProgressEvent::BuildTaskStarted {
            current,
            total,
            description,
            ..
        } => Some(format!(" Task {current}/{total}: {description}")),
        ToolingProgressEvent::BuildTaskFinished {
            current,
            total,
            description,
            status,
            ..
        } => match status {
            ToolingProgressStatus::Cached => {
                Some(format!(" Cached {current}/{total}: {description}"))
            }
            ToolingProgressStatus::Skipped => {
                Some(format!(" Skipped {current}/{total}: {description}"))
            }
            ToolingProgressStatus::Failed => {
                Some(format!(" Build failed {current}/{total}: {description}"))
            }
            ToolingProgressStatus::Planned => {
                Some(format!(" Planned {current}/{total}: {description}"))
            }
            _ => None,
        },
        ToolingProgressEvent::RunPlanReady {
            total_units,
            target,
            workspace_root,
            ..
        } => Some(format!(
            " Planned {total_units} run unit(s) for {target} in {}",
            display_path(workspace_root, cwd)
        )),
        ToolingProgressEvent::RunUnitStarted {
            current,
            total,
            label,
            target,
            ..
        } => Some(format!(" Run {current}/{total}: {label} ({target})")),
        ToolingProgressEvent::RunUnitFinished {
            current,
            total,
            label,
            status,
            ..
        } => match status {
            ToolingProgressStatus::Failed => {
                Some(format!(" Run failed {current}/{total}: {label}"))
            }
            ToolingProgressStatus::Planned => Some(format!(" Planned {current}/{total}: {label}")),
            _ => None,
        },
        ToolingProgressEvent::RunHandOff { label, command, .. } => Some(match command {
            Some(command) => format!(" Executing {command} for {label}"),
            None => format!(" Executing {label}"),
        }),
        ToolingProgressEvent::CompilerPhase {
            source_path, phase, ..
        } => Some(match source_path {
            Some(path) => format!("   {} {}", phase_label(*phase), display_path(path, cwd)),
            None => format!("   {}", phase_label(*phase)),
        }),
    }
}

fn phase_label(phase: CompilerProgressPhase) -> &'static str {
    match phase {
        CompilerProgressPhase::Resolve => "resolve",
        CompilerProgressPhase::Parse => "parse",
        CompilerProgressPhase::Comptime => "comptime",
        CompilerProgressPhase::Typecheck => "typecheck",
        CompilerProgressPhase::Monomorphize => "monomorphize",
        CompilerProgressPhase::Codegen => "codegen",
        CompilerProgressPhase::Interpret => "interpret",
    }
}

fn display_path(path: &Path, cwd: &Path) -> String {
    path.strip_prefix(cwd)
        .map(|relative| relative.display().to_string())
        .unwrap_or_else(|_| path.display().to_string())
}
