use kain_driver::{
    CompilerProgressPhase, ToolingProgressEvent, ToolingProgressSink, ToolingProgressStatus,
};
use kain_lattice::Painter;
use std::io::{self, IsTerminal};
use std::path::{Path, PathBuf};
use std::sync::{Arc, Mutex};

fn active_painter() -> Painter {
    kain_core::tooling_config::active_painter()
}

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
    let p = active_painter();
    match event {
        ToolingProgressEvent::CheckDiscoveryStarted { root, target } => Some(format!(
            "{} Discovering Kain sources under {} for {}...",
            p.status_info(""),
            display_path(root, cwd),
            target
        )),
        ToolingProgressEvent::CheckDiscoveryFinished {
            total_files,
            target,
            ..
        } => Some(format!(
            "{} Checking {total_files} file(s) for {target}...",
            p.status_info("")
        )),
        ToolingProgressEvent::CheckFileStarted {
            current,
            total,
            path,
            ..
        } => Some(format!(
            "{} Checking {}: {}",
            p.status_info(""),
            p.status_info(&format!("{current}/{total}")),
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
                "{} Check failed {}: {}",
                p.status_error(""),
                p.status_error(&format!("{current}/{total}")),
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
            "{} Planned {} build task(s) for {target} in {}",
            p.status_info(""),
            p.status_info(&total_tasks.to_string()),
            display_path(workspace_root, cwd)
        )),
        ToolingProgressEvent::BuildTaskStarted {
            current,
            total,
            description,
            ..
        } => Some(format!(
            "{} Task {}: {description}",
            p.status_info(""),
            p.status_info(&format!("{current}/{total}"))
        )),
        ToolingProgressEvent::BuildTaskFinished {
            current,
            total,
            description,
            status,
            ..
        } => match status {
            ToolingProgressStatus::Cached => Some(format!(
                "{} Cached {}: {description}",
                p.status_cached(""),
                p.status_cached(&format!("{current}/{total}"))
            )),
            ToolingProgressStatus::Skipped => Some(format!(
                "{} Skipped {}: {description}",
                p.status_muted(""),
                p.status_muted(&format!("{current}/{total}"))
            )),
            ToolingProgressStatus::Failed => Some(format!(
                "{} Build failed {}: {description}",
                p.status_error(""),
                p.status_error(&format!("{current}/{total}"))
            )),
            ToolingProgressStatus::Planned => Some(format!(
                "{} Planned {}: {description}",
                p.status_info(""),
                p.status_info(&format!("{current}/{total}"))
            )),
            _ => None,
        },
        ToolingProgressEvent::RunPlanReady {
            total_units,
            target,
            workspace_root,
            ..
        } => Some(format!(
            "{} Planned {} run unit(s) for {target} in {}",
            p.status_info(""),
            p.status_info(&total_units.to_string()),
            display_path(workspace_root, cwd)
        )),
        ToolingProgressEvent::RunUnitStarted {
            current,
            total,
            label,
            target,
            ..
        } => Some(format!(
            "{} Run {}: {label} ({target})",
            p.status_info(""),
            p.status_info(&format!("{current}/{total}"))
        )),
        ToolingProgressEvent::RunUnitFinished {
            current,
            total,
            label,
            status,
            ..
        } => match status {
            ToolingProgressStatus::Failed => Some(format!(
                "{} Run failed {}: {label}",
                p.status_error(""),
                p.status_error(&format!("{current}/{total}"))
            )),
            ToolingProgressStatus::Planned => Some(format!(
                "{} Planned {}: {label}",
                p.status_info(""),
                p.status_info(&format!("{current}/{total}"))
            )),
            _ => None,
        },
        ToolingProgressEvent::RunHandOff { label, command, .. } => Some(match command {
            Some(command) => format!(
                "{} Executing {command} for {label}",
                p.status_info("")
            ),
            None => format!(
                "{} Executing {label}",
                p.status_info("")
            ),
        }),
        ToolingProgressEvent::CompilerPhase {
            source_path, phase, ..
        } => Some(match source_path {
            Some(path) => format!(
                "{}  {} {}",
                p.status_muted(""),
                p.status_muted(phase_label(*phase)),
                display_path(path, cwd)
            ),
            None => format!(
                "{}  {}",
                p.status_muted(""),
                p.status_muted(phase_label(*phase))
            ),
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
