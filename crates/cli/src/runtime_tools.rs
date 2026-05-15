pub use kain_commands::kain::RuntimeCommand;

use std::path::{Path, PathBuf};
use std::process::{Command, ExitStatus};

#[allow(dead_code)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum RuntimeHostPlatform {
    Windows,
    Posix,
}

#[derive(Debug, Clone, PartialEq, Eq)]
struct NativeRuntimeScriptInvocation {
    label: &'static str,
    program: String,
    args: Vec<String>,
    working_dir: PathBuf,
}

pub fn run(command: RuntimeCommand) -> Result<(), String> {
    let repo_root = resolve_runtime_workspace_root()?;
    let (platform, shell_program) = resolve_runtime_shell()?;
    let invocation = build_runtime_invocation(&repo_root, command, platform, &shell_program)?;
    execute_runtime_invocation(&invocation)
}

fn resolve_runtime_workspace_root() -> Result<PathBuf, String> {
    find_runtime_workspace_root().ok_or_else(|| {
        "unable to locate the Kain workspace root for native runtime commands. Run from the repo, use a repo-built kain binary, or set KAIN_REPO_ROOT.".to_string()
    })
}

fn find_runtime_workspace_root() -> Option<PathBuf> {
    let mut candidates = Vec::new();
    if let Ok(env_root) = std::env::var("KAIN_REPO_ROOT") {
        let trimmed = env_root.trim();
        if !trimmed.is_empty() {
            candidates.push(PathBuf::from(trimmed));
        }
    }
    if let Ok(cwd) = std::env::current_dir() {
        candidates.push(cwd);
    }
    if let Ok(exe_path) = std::env::current_exe() {
        if let Some(dir) = exe_path.parent() {
            candidates.push(dir.to_path_buf());
        }
    }
    if let Ok(manifest_dir) = std::env::var("CARGO_MANIFEST_DIR") {
        candidates.push(PathBuf::from(manifest_dir));
    }
    find_runtime_workspace_root_from_candidates(candidates)
}

fn find_runtime_workspace_root_from_candidates<I>(candidates: I) -> Option<PathBuf>
where
    I: IntoIterator<Item = PathBuf>,
{
    for candidate in candidates {
        if let Some(root) = ascend_to_runtime_workspace_root(&candidate) {
            return Some(root);
        }
    }
    None
}

fn ascend_to_runtime_workspace_root(start: &Path) -> Option<PathBuf> {
    let mut cursor = if start.is_file() {
        start.parent()?.to_path_buf()
    } else {
        start.to_path_buf()
    };
    loop {
        if is_runtime_workspace_root(&cursor) {
            return Some(cursor);
        }
        if !cursor.pop() {
            break;
        }
    }
    None
}

fn is_runtime_workspace_root(path: &Path) -> bool {
    let has_runtime_manifest = path
        .join("runtime")
        .join("native_core_runtime.toml")
        .is_file()
        || path.join("runtime").join("native_runtime.toml").is_file();
    path.join("Cargo.toml").is_file()
        && has_runtime_manifest
        && path
            .join("runtime")
            .join("compile_native_runtime.sh")
            .is_file()
        && path
            .join("runtime")
            .join("validate_native_runtime.sh")
            .is_file()
}

fn resolve_runtime_shell() -> Result<(RuntimeHostPlatform, String), String> {
    #[cfg(target_os = "windows")]
    {
        let program = which::which("pwsh")
            .or_else(|_| which::which("powershell"))
            .map_err(|_| {
                "unable to find PowerShell for native runtime commands. Install `pwsh` or ensure `powershell` is on PATH.".to_string()
            })?;
        Ok((
            RuntimeHostPlatform::Windows,
            program.to_string_lossy().into_owned(),
        ))
    }

    #[cfg(not(target_os = "windows"))]
    {
        let program = which::which("bash").map_err(|_| {
            "unable to find `bash` for native runtime commands. Install bash or add it to PATH."
                .to_string()
        })?;
        Ok((
            RuntimeHostPlatform::Posix,
            program.to_string_lossy().into_owned(),
        ))
    }
}

fn build_runtime_invocation(
    repo_root: &Path,
    command: RuntimeCommand,
    platform: RuntimeHostPlatform,
    shell_program: &str,
) -> Result<NativeRuntimeScriptInvocation, String> {
    match platform {
        RuntimeHostPlatform::Windows => {
            build_windows_runtime_invocation(repo_root, command, shell_program)
        }
        RuntimeHostPlatform::Posix => {
            build_posix_runtime_invocation(repo_root, command, shell_program)
        }
    }
}

fn build_windows_runtime_invocation(
    repo_root: &Path,
    command: RuntimeCommand,
    shell_program: &str,
) -> Result<NativeRuntimeScriptInvocation, String> {
    let runtime_dir = repo_root.join("runtime");
    match command {
        RuntimeCommand::Build { release, verbose } => {
            let script_path = runtime_dir.join("compile_native_runtime.ps1");
            ensure_runtime_script_exists(&script_path, "PowerShell runtime build wrapper")?;
            let mut args = vec![
                "-ExecutionPolicy".to_string(),
                "Bypass".to_string(),
                "-File".to_string(),
                script_path.display().to_string(),
            ];
            if release {
                args.push("-Release".to_string());
            }
            if verbose {
                args.push("-ScriptVerbose".to_string());
            }
            Ok(NativeRuntimeScriptInvocation {
                label: "kain runtime build",
                program: shell_program.to_string(),
                args,
                working_dir: repo_root.to_path_buf(),
            })
        }
        RuntimeCommand::Validate {
            release,
            verbose,
            skip_cli_build,
            skip_runtime_build,
            skip_fixtures,
            skip_conformance,
        } => {
            let script_path = runtime_dir.join("validate_native_runtime.ps1");
            ensure_runtime_script_exists(&script_path, "PowerShell runtime validation wrapper")?;
            let mut args = vec![
                "-ExecutionPolicy".to_string(),
                "Bypass".to_string(),
                "-File".to_string(),
                script_path.display().to_string(),
            ];
            if release {
                args.push("-Release".to_string());
            }
            if verbose {
                args.push("-ScriptVerbose".to_string());
            }
            if skip_cli_build {
                args.push("-SkipCliBuild".to_string());
            }
            if skip_runtime_build {
                args.push("-SkipRuntimeBuild".to_string());
            }
            if skip_fixtures {
                args.push("-SkipFixtures".to_string());
            }
            if skip_conformance {
                args.push("-SkipConformance".to_string());
            }
            Ok(NativeRuntimeScriptInvocation {
                label: "kain runtime validate",
                program: shell_program.to_string(),
                args,
                working_dir: repo_root.to_path_buf(),
            })
        }
    }
}

fn build_posix_runtime_invocation(
    repo_root: &Path,
    command: RuntimeCommand,
    shell_program: &str,
) -> Result<NativeRuntimeScriptInvocation, String> {
    let runtime_dir = repo_root.join("runtime");
    match command {
        RuntimeCommand::Build { release, verbose } => {
            let script_path = runtime_dir.join("compile_native_runtime.sh");
            ensure_runtime_script_exists(&script_path, "bash runtime build wrapper")?;
            let mut args = vec![script_path.display().to_string()];
            if release {
                args.push("--release".to_string());
            }
            if verbose {
                args.push("--verbose".to_string());
            }
            Ok(NativeRuntimeScriptInvocation {
                label: "kain runtime build",
                program: shell_program.to_string(),
                args,
                working_dir: repo_root.to_path_buf(),
            })
        }
        RuntimeCommand::Validate {
            release,
            verbose,
            skip_cli_build,
            skip_runtime_build,
            skip_fixtures,
            skip_conformance,
        } => {
            let script_path = runtime_dir.join("validate_native_runtime.sh");
            ensure_runtime_script_exists(&script_path, "bash runtime validation wrapper")?;
            let mut args = vec![script_path.display().to_string()];
            if release {
                args.push("--release".to_string());
            }
            if verbose {
                args.push("--verbose".to_string());
            }
            if skip_cli_build {
                args.push("--skip-cli-build".to_string());
            }
            if skip_runtime_build {
                args.push("--skip-runtime-build".to_string());
            }
            if skip_fixtures {
                args.push("--skip-fixtures".to_string());
            }
            if skip_conformance {
                args.push("--skip-conformance".to_string());
            }
            Ok(NativeRuntimeScriptInvocation {
                label: "kain runtime validate",
                program: shell_program.to_string(),
                args,
                working_dir: repo_root.to_path_buf(),
            })
        }
    }
}

fn ensure_runtime_script_exists(path: &Path, label: &str) -> Result<(), String> {
    if path.is_file() {
        Ok(())
    } else {
        Err(format!("missing {}: {}", label, path.display()))
    }
}

fn execute_runtime_invocation(invocation: &NativeRuntimeScriptInvocation) -> Result<(), String> {
    let status = Command::new(&invocation.program)
        .args(&invocation.args)
        .current_dir(&invocation.working_dir)
        .status()
        .map_err(|err| {
            format!(
                "failed to launch {} via {}: {}",
                invocation.label, invocation.program, err
            )
        })?;
    if status.success() {
        Ok(())
    } else {
        Err(format!(
            "{} failed with {}",
            invocation.label,
            format_exit_status(status)
        ))
    }
}

fn format_exit_status(status: ExitStatus) -> String {
    match status.code() {
        Some(code) => format!("exit code {code}"),
        None => "a terminated process".to_string(),
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use tempfile::TempDir;

    #[test]
    fn runtime_workspace_root_detection_finds_repo_from_nested_candidate() {
        let temp = TempDir::new().expect("temp dir");
        let repo_root = seed_runtime_workspace(temp.path());
        let nested = repo_root.join("crates").join("cli").join("src");

        let resolved =
            find_runtime_workspace_root_from_candidates([nested]).expect("runtime workspace root");

        assert_eq!(resolved, repo_root);
    }

    #[test]
    fn build_runtime_invocation_uses_runtime_wrapper_flags() {
        let temp = TempDir::new().expect("temp dir");
        let repo_root = seed_runtime_workspace(temp.path());

        let invocation = build_runtime_invocation(
            &repo_root,
            RuntimeCommand::Build {
                release: true,
                verbose: true,
            },
            RuntimeHostPlatform::Windows,
            "powershell",
        )
        .expect("invocation");

        assert_eq!(invocation.label, "kain runtime build");
        assert_eq!(invocation.program, "powershell");
        assert_eq!(
            invocation.args,
            vec![
                "-ExecutionPolicy".to_string(),
                "Bypass".to_string(),
                "-File".to_string(),
                repo_root
                    .join("runtime")
                    .join("compile_native_runtime.ps1")
                    .display()
                    .to_string(),
                "-Release".to_string(),
                "-ScriptVerbose".to_string(),
            ]
        );
        assert_eq!(invocation.working_dir, repo_root);
    }

    #[test]
    fn validate_runtime_invocation_forwards_skip_flags() {
        let temp = TempDir::new().expect("temp dir");
        let repo_root = seed_runtime_workspace(temp.path());

        let invocation = build_runtime_invocation(
            &repo_root,
            RuntimeCommand::Validate {
                release: false,
                verbose: true,
                skip_cli_build: true,
                skip_runtime_build: true,
                skip_fixtures: true,
                skip_conformance: true,
            },
            RuntimeHostPlatform::Posix,
            "bash",
        )
        .expect("invocation");

        assert_eq!(invocation.label, "kain runtime validate");
        assert_eq!(invocation.program, "bash");
        assert_eq!(
            invocation.args,
            vec![
                repo_root
                    .join("runtime")
                    .join("validate_native_runtime.sh")
                    .display()
                    .to_string(),
                "--verbose".to_string(),
                "--skip-cli-build".to_string(),
                "--skip-runtime-build".to_string(),
                "--skip-fixtures".to_string(),
                "--skip-conformance".to_string(),
            ]
        );
        assert_eq!(invocation.working_dir, repo_root);
    }

    fn seed_runtime_workspace(root: &Path) -> PathBuf {
        let repo_root = root.join("kain-repo");
        std::fs::create_dir_all(repo_root.join("runtime")).expect("runtime dir");
        std::fs::create_dir_all(repo_root.join("crates").join("cli").join("src"))
            .expect("nested dirs");
        std::fs::write(repo_root.join("Cargo.toml"), "[workspace]\n").expect("workspace manifest");
        std::fs::write(
            repo_root.join("runtime").join("native_core_runtime.toml"),
            "sources = []\n",
        )
        .expect("runtime manifest");
        std::fs::write(
            repo_root.join("runtime").join("compile_native_runtime.sh"),
            "#!/usr/bin/env bash\n",
        )
        .expect("compile script");
        std::fs::write(
            repo_root.join("runtime").join("compile_native_runtime.ps1"),
            "Write-Host 'compile'\n",
        )
        .expect("compile wrapper");
        std::fs::write(
            repo_root.join("runtime").join("validate_native_runtime.sh"),
            "#!/usr/bin/env bash\n",
        )
        .expect("validate script");
        std::fs::write(
            repo_root
                .join("runtime")
                .join("validate_native_runtime.ps1"),
            "Write-Host 'validate'\n",
        )
        .expect("validate wrapper");
        repo_root
    }
}
