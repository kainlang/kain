use kain_clean::{CleanScope, WorkspaceCleanOptions, WorkspaceCleanReport};
use serde::Serialize;
use std::path::PathBuf;

pub fn run_workspace_clean(
    path: PathBuf,
    scope: String,
    dry_run: bool,
    json: bool,
) -> Result<(), String> {
    let scope = CleanScope::parse(&scope).ok_or_else(|| {
        format!(
            "unknown clean scope '{}'; expected build, run, amalgamate, or all",
            scope
        )
    })?;
    let mut options = WorkspaceCleanOptions::new(path);
    options.scope = scope;
    options.dry_run = dry_run;
    let report =
        kain_clean::execute_workspace_clean(&options).map_err(|error| error.to_string())?;
    if json {
        print_json(&report)
    } else {
        print_report(&report);
        Ok(())
    }
}

fn print_json<T: Serialize>(value: &T) -> Result<(), String> {
    let text = serde_json::to_string_pretty(value)
        .map_err(|err| format!("failed to serialize clean JSON: {err}"))?;
    println!("{text}");
    Ok(())
}

fn print_report(report: &WorkspaceCleanReport) {
    println!("Kain clean: {}", report.workspace_root.display());
    println!("  scope: {}", report.scope.as_str());
    println!(
        "  mode: {}",
        if report.dry_run { "dry-run" } else { "execute" }
    );
    for action in &report.actions {
        let status = if action.existed {
            if report.dry_run {
                "would-remove"
            } else if action.removed {
                "removed"
            } else {
                "present"
            }
        } else {
            "missing"
        };
        println!(
            "    {} {} {}",
            status,
            action.kind.as_str(),
            action.path.display()
        );
    }
    println!(
        "  note: build cache reuse still stays guarded by SHA-256 task stamps and runtime cache fingerprints."
    );
}
