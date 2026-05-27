use std::env;
use std::ffi::{OsStr, OsString};
use std::path::{Path, PathBuf};
use std::process::{self, Command};

const DEFAULT_REPO_ROOT: &str = env!("KAIN_DEFAULT_REPO_ROOT");
const DEFAULT_BAZEL_CONFIG: &str = match option_env!("KAIN_DEFAULT_BAZEL_CONFIG") {
    Some(value) if !value.is_empty() => value,
    _ => "dev",
};
const DEFAULT_LAUNCHER_DIR: &str = ".kain/bin";

fn is_non_empty(value: &OsStr) -> bool {
    !value.is_empty()
}

fn resolve_repo_root() -> PathBuf {
    if let Some(value) = env::var_os("KAIN_REPO_ROOT").filter(|value| is_non_empty(value)) {
        return PathBuf::from(value);
    }
    PathBuf::from(DEFAULT_REPO_ROOT)
}

fn resolve_bazel_config() -> OsString {
    env::var_os("KAIN_BAZEL_CONFIG")
        .filter(|value| is_non_empty(value))
        .unwrap_or_else(|| OsString::from(DEFAULT_BAZEL_CONFIG))
}

fn resolve_launcher_dir(repo_root: &Path) -> PathBuf {
    let configured = env::var_os("KAIN_BAZEL_LAUNCHER_DIR")
        .filter(|value| is_non_empty(value))
        .map(PathBuf::from)
        .or_else(|| {
            option_env!("KAIN_DEFAULT_LAUNCHER_DIR")
                .filter(|value| !value.is_empty())
                .map(PathBuf::from)
        })
        .unwrap_or_else(|| PathBuf::from(DEFAULT_LAUNCHER_DIR));

    if configured.is_absolute() {
        configured
    } else {
        repo_root.join(configured)
    }
}

fn resolve_binary_name(exe_path: &Path) -> Result<&'static str, String> {
    let stem = exe_path
        .file_stem()
        .and_then(|value| value.to_str())
        .ok_or_else(|| format!("unable to derive launcher name from {}", exe_path.display()))?;

    match stem.to_ascii_lowercase().as_str() {
        "kain" => Ok("kain"),
        "kn" => Ok("kn"),
        "blade" => Err(
            "the standalone blade launcher was removed; use `kain` or `kn` from the project root instead"
                .to_string(),
        ),
        other => Err(format!(
            "unsupported launcher name `{other}` at {}",
            exe_path.display()
        )),
    }
}

fn python_commands() -> Vec<(OsString, Vec<OsString>)> {
    let mut candidates = Vec::new();
    if let Some(value) = env::var_os("KAIN_BAZEL_PYTHON").filter(|value| is_non_empty(value)) {
        candidates.push((value, Vec::new()));
    }

    if cfg!(windows) {
        candidates.push((OsString::from("py"), vec![OsString::from("-3")]));
        candidates.push((OsString::from("python"), Vec::new()));
    } else {
        candidates.push((OsString::from("python3"), Vec::new()));
        candidates.push((OsString::from("python"), Vec::new()));
    }
    candidates
}

fn main() {
    let launcher_path = match env::current_exe() {
        Ok(path) => path,
        Err(error) => {
            eprintln!("kain bazel launcher failed to resolve its own path: {error}");
            process::exit(1);
        }
    };

    let binary_name = match resolve_binary_name(&launcher_path) {
        Ok(name) => name,
        Err(message) => {
            eprintln!("{message}");
            process::exit(1);
        }
    };

    let repo_root = resolve_repo_root();
    let launcher_script = repo_root
        .join("scripts")
        .join("python")
        .join("kain_bazel_sync.py");
    if !launcher_script.exists() {
        eprintln!(
            "kain bazel launcher could not find {}",
            launcher_script.display()
        );
        process::exit(1);
    }

    let forward_args: Vec<OsString> = env::args_os().skip(1).collect();
    let bazel_config = resolve_bazel_config();
    let launcher_dir = resolve_launcher_dir(&repo_root);
    let mut last_error = None;
    for (python_program, python_args) in python_commands() {
        let status = Command::new(&python_program)
            .args(&python_args)
            .arg(&launcher_script)
            .arg("launch")
            .arg("--binary")
            .arg(binary_name)
            .arg("--bazel-config")
            .arg(&bazel_config)
            .arg("--launcher-path")
            .arg(&launcher_path)
            .arg("--")
            .args(&forward_args)
            .env("KAIN_REPO_ROOT", &repo_root)
            .env("KAIN_BAZEL_CONFIG", &bazel_config)
            .env("KAIN_BAZEL_LAUNCHER_DIR", &launcher_dir)
            .env("KAIN_ACTIVE_LAUNCHER_NAME", binary_name)
            .env("KAIN_ACTIVE_LAUNCHER_MODE", "bazel-wrapper")
            .env("KAIN_ACTIVE_LAUNCHER_PATH", &launcher_path)
            .status();

        match status {
            Ok(status) => process::exit(status.code().unwrap_or(1)),
            Err(error) => {
                last_error = Some(format!("{:?}: {error}", python_program));
            }
        }
    }

    eprintln!(
        "kain bazel launcher could not start Python for {}{}",
        launcher_script.display(),
        last_error
            .map(|error| format!("; last error was {error}"))
            .unwrap_or_default()
    );
    process::exit(1);
}
