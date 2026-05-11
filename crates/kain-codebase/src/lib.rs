use kain_core::error::{KainError, KainResult};
use kain_core::runtime::{register_env_extension, Env, Value};
use kain_core::stdlib::{register_stdlib_extension, BuiltinFn, StdLib};
use kain_fs::{self as kfs, FsFileType};
use libloading::Library;
use once_cell::sync::Lazy;
use serde::{Deserialize, Serialize};
use serde_json::{json, Map, Value as JsonValue};
use std::collections::HashMap;
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};
use std::sync::{Arc, Once, RwLock};

const EXTENSION_KEY: &str = "kain_codebase";
const DEFAULT_SCAN_LIMIT: usize = 10_000;
const DEFAULT_TRUST_MODE: TrustMode = TrustMode::TrustedLocal;

static REGISTER: Once = Once::new();

static ROOT_MARKERS: &[&str] = &["KAIN.toml", "package.json", "Cargo.toml", ".git"];
static DEFAULT_SCAN_IGNORES: Lazy<Vec<&'static str>> =
    Lazy::new(|| vec![".git", "node_modules", "target", ".kain"]);

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "kebab-case")]
pub enum TrustMode {
    TrustedLocal,
    Reviewed,
}

impl Default for TrustMode {
    fn default() -> Self {
        DEFAULT_TRUST_MODE
    }
}

impl TrustMode {
    pub fn as_str(self) -> &'static str {
        match self {
            Self::TrustedLocal => "trusted-local",
            Self::Reviewed => "reviewed",
        }
    }
}

#[derive(Debug, Clone)]
pub struct CodebaseConfig {
    pub trust_mode: TrustMode,
}

impl Default for CodebaseConfig {
    fn default() -> Self {
        Self {
            trust_mode: TrustMode::TrustedLocal,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WorkspaceInspection {
    pub input_path: PathBuf,
    pub root: PathBuf,
    pub trust_mode: TrustMode,
    pub markers: Vec<String>,
    pub has_kain_manifest: bool,
    pub has_package_json: bool,
    pub has_cargo_manifest: bool,
    pub has_git: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CodebaseFileEntry {
    pub path: PathBuf,
    pub relative_path: PathBuf,
    pub kind: String,
    pub byte_len: u64,
    pub extension: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CodebaseRunResult {
    pub command: String,
    pub args: Vec<String>,
    pub cwd: PathBuf,
    pub status: Option<i32>,
    pub success: bool,
    pub stdout: String,
    pub stderr: String,
}

pub fn register() {
    REGISTER.call_once(|| {
        register_stdlib_extension(EXTENSION_KEY, register_codebase_stdlib);
        register_env_extension(EXTENSION_KEY, register_codebase_env);
    });
}

pub fn inspect_workspace(path: impl AsRef<Path>) -> KainResult<WorkspaceInspection> {
    let input_path = normalize_path_for_display(path.as_ref())?;
    let root = discover_workspace_root(path.as_ref())?;
    let markers = ROOT_MARKERS
        .iter()
        .filter(|marker| root.join(marker).exists())
        .map(|marker| (*marker).to_string())
        .collect::<Vec<_>>();
    Ok(WorkspaceInspection {
        input_path,
        has_kain_manifest: root.join("KAIN.toml").exists(),
        has_package_json: root.join("package.json").exists(),
        has_cargo_manifest: root.join("Cargo.toml").exists(),
        has_git: root.join(".git").exists(),
        root,
        trust_mode: TrustMode::TrustedLocal,
        markers,
    })
}

pub fn discover_workspace_root(path: impl AsRef<Path>) -> KainResult<PathBuf> {
    let mut current = existing_directory_anchor(path.as_ref())?;
    loop {
        if ROOT_MARKERS
            .iter()
            .any(|marker| current.join(marker).exists())
        {
            return Ok(current);
        }
        let Some(parent) = current.parent() else {
            return existing_directory_anchor(path.as_ref());
        };
        current = parent.to_path_buf();
    }
}

pub fn read_text(path: impl AsRef<Path>) -> KainResult<String> {
    kfs::read_text(path.as_ref()).map_err(fs_to_kain_error)
}

pub fn write_text(path: impl AsRef<Path>, content: &str) -> KainResult<()> {
    kfs::write_text(path.as_ref(), content).map_err(fs_to_kain_error)
}

pub fn delete_path(path: impl AsRef<Path>) -> KainResult<()> {
    let path = path.as_ref();
    if !path.exists() {
        return Ok(());
    }
    if path.is_dir() {
        kfs::remove_dir_all(path).map_err(fs_to_kain_error)
    } else {
        kfs::remove_file(path).map_err(fs_to_kain_error)
    }
}

pub fn copy_path(source: impl AsRef<Path>, destination: impl AsRef<Path>) -> KainResult<()> {
    kfs::copy_path(source.as_ref(), destination.as_ref()).map_err(fs_to_kain_error)
}

pub fn move_path(source: impl AsRef<Path>, destination: impl AsRef<Path>) -> KainResult<()> {
    kfs::move_path(source.as_ref(), destination.as_ref()).map_err(fs_to_kain_error)
}

pub fn create_directory(path: impl AsRef<Path>) -> KainResult<()> {
    kfs::create_dir_all(path.as_ref()).map_err(fs_to_kain_error)
}

pub fn read_json_file(path: impl AsRef<Path>) -> KainResult<JsonValue> {
    let source = read_text(path)?;
    serde_json::from_str(&source)
        .map_err(|err| KainError::runtime(format!("Failed to parse JSON: {err}")))
}

pub fn write_json_file(path: impl AsRef<Path>, value: &JsonValue) -> KainResult<()> {
    let text = serde_json::to_string_pretty(value)
        .map_err(|err| KainError::runtime(format!("Failed to encode JSON: {err}")))?;
    write_text(path, &(text + "\n"))
}

pub fn read_toml_file(path: impl AsRef<Path>) -> KainResult<JsonValue> {
    let source = read_text(path)?;
    let value: toml::Value = toml::from_str(&source)
        .map_err(|err| KainError::runtime(format!("Failed to parse TOML: {err}")))?;
    serde_json::to_value(value)
        .map_err(|err| KainError::runtime(format!("Failed to project TOML: {err}")))
}

pub fn write_toml_file(path: impl AsRef<Path>, value: &JsonValue) -> KainResult<()> {
    let text = toml::to_string_pretty(value)
        .map_err(|err| KainError::runtime(format!("Failed to encode TOML: {err}")))?;
    write_text(path, &text)
}

pub fn scan_path(path: impl AsRef<Path>) -> KainResult<Vec<CodebaseFileEntry>> {
    let root = existing_directory_anchor(path.as_ref())?;
    let mut entries = Vec::new();
    scan_path_inner(&root, &root, &mut entries, DEFAULT_SCAN_LIMIT)?;
    entries.sort_by(|left, right| left.relative_path.cmp(&right.relative_path));
    Ok(entries)
}

pub fn hash_file(path: impl AsRef<Path>) -> KainResult<String> {
    kfs::hash_file(path.as_ref()).map_err(fs_to_kain_error)
}

pub fn run_command(
    cwd: impl AsRef<Path>,
    command: &str,
    args: &[String],
) -> KainResult<CodebaseRunResult> {
    let cwd = cwd.as_ref().to_path_buf();
    let resolved_command = resolve_command_for_spawn(command);
    let output = Command::new(&resolved_command)
        .args(args)
        .current_dir(&cwd)
        .stdin(Stdio::null())
        .output()
        .map_err(|err| {
            KainError::runtime(format!(
                "Failed to run command '{}' in '{}': {err}",
                command,
                cwd.display()
            ))
        })?;
    Ok(CodebaseRunResult {
        command: command.to_string(),
        args: args.to_vec(),
        cwd,
        status: output.status.code(),
        success: output.status.success(),
        stdout: String::from_utf8_lossy(&output.stdout).to_string(),
        stderr: String::from_utf8_lossy(&output.stderr).to_string(),
    })
}

pub fn cargo_workspace(path: impl AsRef<Path>) -> KainResult<JsonValue> {
    let root = discover_workspace_root(path)?;
    let manifest_path = root.join("Cargo.toml");
    if !manifest_path.exists() {
        return Err(KainError::runtime(format!(
            "No Cargo.toml found at discovered root '{}'",
            root.display()
        )));
    }
    read_toml_file(manifest_path)
}

pub fn cargo_run(
    cwd: impl AsRef<Path>,
    subcommand: &str,
    args: &[String],
) -> KainResult<CodebaseRunResult> {
    let mut cargo_args = Vec::with_capacity(args.len() + 1);
    cargo_args.push(subcommand.to_string());
    cargo_args.extend(args.iter().cloned());
    run_command(cwd, "cargo", &cargo_args)
}

pub fn cargo_import_crate(
    cwd: impl AsRef<Path>,
    crate_name: &str,
    args: &[String],
) -> KainResult<CodebaseRunResult> {
    let mut kain_args = Vec::with_capacity(args.len() + 2);
    kain_args.push("import-crate".to_string());
    kain_args.push(crate_name.to_string());
    kain_args.extend(args.iter().cloned());
    run_current_kain(cwd, &kain_args)
}

pub fn python_run(
    cwd: impl AsRef<Path>,
    script_or_module: &str,
    args: &[String],
) -> KainResult<CodebaseRunResult> {
    let mut python_args = Vec::with_capacity(args.len() + 1);
    python_args.push(script_or_module.to_string());
    python_args.extend(args.iter().cloned());
    run_command(cwd, "python", &python_args)
}

pub fn python_import(cwd: impl AsRef<Path>, module: &str) -> KainResult<JsonValue> {
    let script = "import importlib, json, sys\nm = importlib.import_module(sys.argv[1])\nprint(json.dumps({'module': sys.argv[1], 'file': getattr(m, '__file__', None), 'package': getattr(m, '__package__', None)}))";
    let args = vec!["-c".to_string(), script.to_string(), module.to_string()];
    let result = run_command(cwd, "python", &args)?;
    parse_successful_json_command("python_import", result)
}

pub fn python_call(
    cwd: impl AsRef<Path>,
    module: &str,
    function: &str,
    args: &JsonValue,
) -> KainResult<JsonValue> {
    let script = "import importlib, json, sys\nm = importlib.import_module(sys.argv[1])\nf = getattr(m, sys.argv[2])\nargs = json.loads(sys.stdin.read() or '[]')\nprint(json.dumps(f(*args)))";
    let cwd = cwd.as_ref().to_path_buf();
    let mut child = Command::new("python")
        .args(["-c", script, module, function])
        .current_dir(&cwd)
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()
        .map_err(|err| KainError::runtime(format!("Failed to spawn python_call: {err}")))?;
    if let Some(stdin) = child.stdin.as_mut() {
        use std::io::Write;
        let payload = serde_json::to_vec(args).map_err(|err| {
            KainError::runtime(format!("Failed to encode python_call args: {err}"))
        })?;
        stdin.write_all(&payload).map_err(KainError::Io)?;
    }
    let output = child
        .wait_with_output()
        .map_err(|err| KainError::runtime(format!("Failed to wait for python_call: {err}")))?;
    let result = CodebaseRunResult {
        command: "python".to_string(),
        args: vec![
            "-c".to_string(),
            "<python_call>".to_string(),
            module.to_string(),
            function.to_string(),
        ],
        cwd,
        status: output.status.code(),
        success: output.status.success(),
        stdout: String::from_utf8_lossy(&output.stdout).to_string(),
        stderr: String::from_utf8_lossy(&output.stderr).to_string(),
    };
    parse_successful_json_command("python_call", result)
}

pub fn compile_c_shared(
    cwd: impl AsRef<Path>,
    source: &str,
    output: &str,
    extra_args: &[String],
) -> KainResult<CodebaseRunResult> {
    let compiler = std::env::var("CC").unwrap_or_else(|_| "cc".to_string());
    let mut args = Vec::new();
    args.push("-shared".to_string());
    args.push(source.to_string());
    args.push("-o".to_string());
    args.push(output.to_string());
    args.extend(extra_args.iter().cloned());
    run_command(cwd, &compiler, &args)
}

pub fn load_c_library(path: impl AsRef<Path>) -> KainResult<JsonValue> {
    let path = normalize_path_for_display(path.as_ref())?;
    if !path.exists() {
        return Err(KainError::runtime(format!(
            "C library '{}' does not exist",
            path.display()
        )));
    }
    Ok(json!({
        "runtime": "c",
        "library": path,
        "status": "loadable",
        "trust_mode": TrustMode::TrustedLocal.as_str()
    }))
}

pub fn call_c_symbol(
    library_path: impl AsRef<Path>,
    symbol: &str,
    signature: &str,
    args: &JsonValue,
) -> KainResult<JsonValue> {
    let library_path = normalize_path_for_display(library_path.as_ref())?;
    let arg_values = args
        .as_array()
        .ok_or_else(|| KainError::runtime("c_call expects args to be a JSON array"))?;
    unsafe {
        let library = Library::new(&library_path).map_err(|err| {
            KainError::runtime(format!(
                "Failed to load C library '{}': {err}",
                library_path.display()
            ))
        })?;
        match normalize_signature(signature).as_str() {
            "i64()" => {
                let func: libloading::Symbol<unsafe extern "C" fn() -> i64> =
                    library.get(symbol.as_bytes()).map_err(c_symbol_error)?;
                Ok(json!(func()))
            }
            "i64(i64)" => {
                let func: libloading::Symbol<unsafe extern "C" fn(i64) -> i64> =
                    library.get(symbol.as_bytes()).map_err(c_symbol_error)?;
                Ok(json!(func(json_i64_arg(arg_values, 0, "c_call")?)))
            }
            "i64(i64,i64)" => {
                let func: libloading::Symbol<unsafe extern "C" fn(i64, i64) -> i64> =
                    library.get(symbol.as_bytes()).map_err(c_symbol_error)?;
                Ok(json!(func(
                    json_i64_arg(arg_values, 0, "c_call")?,
                    json_i64_arg(arg_values, 1, "c_call")?
                )))
            }
            "f64()" => {
                let func: libloading::Symbol<unsafe extern "C" fn() -> f64> =
                    library.get(symbol.as_bytes()).map_err(c_symbol_error)?;
                Ok(json!(func()))
            }
            "f64(f64)" => {
                let func: libloading::Symbol<unsafe extern "C" fn(f64) -> f64> =
                    library.get(symbol.as_bytes()).map_err(c_symbol_error)?;
                Ok(json!(func(json_f64_arg(arg_values, 0, "c_call")?)))
            }
            "f64(f64,f64)" => {
                let func: libloading::Symbol<unsafe extern "C" fn(f64, f64) -> f64> =
                    library.get(symbol.as_bytes()).map_err(c_symbol_error)?;
                Ok(json!(func(
                    json_f64_arg(arg_values, 0, "c_call")?,
                    json_f64_arg(arg_values, 1, "c_call")?
                )))
            }
            "void()" => {
                let func: libloading::Symbol<unsafe extern "C" fn()> =
                    library.get(symbol.as_bytes()).map_err(c_symbol_error)?;
                func();
                Ok(JsonValue::Null)
            }
            other => Err(KainError::runtime(format!(
                "Unsupported c_call signature '{other}'. Supported: i64(), i64(i64), i64(i64,i64), f64(), f64(f64), f64(f64,f64), void()"
            ))),
        }
    }
}

pub fn ts_import(
    cwd: impl AsRef<Path>,
    input: &str,
    output: &str,
    args: &[String],
) -> KainResult<CodebaseRunResult> {
    let mut kain_args = Vec::with_capacity(args.len() + 4);
    kain_args.push("import-ts".to_string());
    kain_args.push(input.to_string());
    kain_args.push("--output".to_string());
    kain_args.push(output.to_string());
    kain_args.extend(args.iter().cloned());
    run_current_kain(cwd, &kain_args)
}

pub fn ts_compile(
    cwd: impl AsRef<Path>,
    command: &str,
    args: &[String],
) -> KainResult<CodebaseRunResult> {
    run_command(cwd, command, args)
}

fn run_current_kain(cwd: impl AsRef<Path>, args: &[String]) -> KainResult<CodebaseRunResult> {
    let exe = std::env::var("KAIN_BIN")
        .ok()
        .map(PathBuf::from)
        .or_else(|| std::env::current_exe().ok())
        .ok_or_else(|| KainError::runtime("Unable to resolve current Kain executable"))?;
    run_command(cwd, &exe.display().to_string(), args)
}

fn resolve_command_for_spawn(command: &str) -> String {
    if command.contains('/') || command.contains('\\') || Path::new(command).extension().is_some() {
        return command.to_string();
    }
    #[cfg(windows)]
    {
        if let Some(path) = resolve_windows_path_command(command) {
            return path.display().to_string();
        }
    }
    command.to_string()
}

#[cfg(windows)]
fn resolve_windows_path_command(command: &str) -> Option<PathBuf> {
    let path_exts = std::env::var("PATHEXT")
        .unwrap_or_else(|_| ".COM;.EXE;.BAT;.CMD".to_string())
        .split(';')
        .filter(|value| !value.trim().is_empty())
        .map(|value| value.trim().to_string())
        .collect::<Vec<_>>();
    let path = std::env::var_os("PATH")?;
    for root in std::env::split_paths(&path) {
        for ext in &path_exts {
            let candidate = root.join(format!("{command}{ext}"));
            if candidate.is_file() {
                return Some(candidate);
            }
            let lower_candidate = root.join(format!("{}{}", command, ext.to_ascii_lowercase()));
            if lower_candidate.is_file() {
                return Some(lower_candidate);
            }
        }
        let direct = root.join(command);
        if direct.is_file() {
            return Some(direct);
        }
    }
    None
}

fn register_codebase_stdlib(stdlib: &mut StdLib) {
    for builtin in [
        BuiltinFn {
            name: "codebase_find_root",
            params: vec![("path", "String")],
            return_type: "String",
            doc: "Discover a trusted local workspace root by KAIN.toml, package.json, Cargo.toml, or .git",
        },
        BuiltinFn {
            name: "codebase_inspect",
            params: vec![("path", "String")],
            return_type: "Any",
            doc: "Inspect a trusted local workspace root and its marker files",
        },
        BuiltinFn {
            name: "codebase_read",
            params: vec![("path", "String")],
            return_type: "String",
            doc: "Read a UTF-8 text file from the local workspace",
        },
        BuiltinFn {
            name: "codebase_write",
            params: vec![("path", "String"), ("content", "String")],
            return_type: "Unit",
            doc: "Write a UTF-8 text file, creating parent directories",
        },
        BuiltinFn {
            name: "codebase_delete",
            params: vec![("path", "String")],
            return_type: "Unit",
            doc: "Delete a file or directory tree in trusted-local mode",
        },
        BuiltinFn {
            name: "codebase_scan",
            params: vec![("path", "String")],
            return_type: "Any",
            doc: "Recursively scan a workspace path with default build/vendor ignores",
        },
        BuiltinFn {
            name: "codebase_read_json",
            params: vec![("path", "String")],
            return_type: "Any",
            doc: "Read and parse a JSON file",
        },
        BuiltinFn {
            name: "codebase_write_json",
            params: vec![("path", "String"), ("value", "Any")],
            return_type: "Unit",
            doc: "Write a pretty JSON file",
        },
        BuiltinFn {
            name: "codebase_read_toml",
            params: vec![("path", "String")],
            return_type: "Any",
            doc: "Read and parse a TOML file",
        },
        BuiltinFn {
            name: "codebase_write_toml",
            params: vec![("path", "String"), ("value", "Any")],
            return_type: "Unit",
            doc: "Write a TOML file from a Kain object",
        },
        BuiltinFn {
            name: "codebase_hash",
            params: vec![("path", "String")],
            return_type: "String",
            doc: "Hash a file with SHA-256",
        },
        BuiltinFn {
            name: "codebase_run",
            params: vec![("cwd", "String"), ("command", "String"), ("args", "Any")],
            return_type: "Any",
            doc: "Run a local toolchain command with structured stdout/stderr/status capture",
        },
        BuiltinFn {
            name: "cargo_workspace",
            params: vec![("path", "String")],
            return_type: "Any",
            doc: "Discover and parse the nearest Cargo workspace manifest",
        },
        BuiltinFn {
            name: "cargo_run",
            params: vec![("cwd", "String"), ("subcommand", "String"), ("args", "Any")],
            return_type: "Any",
            doc: "Run a Cargo subcommand from Kain",
        },
        BuiltinFn {
            name: "cargo_import_crate",
            params: vec![("cwd", "String"), ("crate_name", "String"), ("args", "Any")],
            return_type: "Any",
            doc: "Run Kain's Rust crate importer from Kain",
        },
        BuiltinFn {
            name: "python_import",
            params: vec![("cwd", "String"), ("module", "String")],
            return_type: "Any",
            doc: "Import a Python package and return module metadata",
        },
        BuiltinFn {
            name: "python_call",
            params: vec![("cwd", "String"), ("module", "String"), ("function", "String"), ("args", "Any")],
            return_type: "Any",
            doc: "Call a Python function with JSON-compatible arguments",
        },
        BuiltinFn {
            name: "python_run",
            params: vec![("cwd", "String"), ("script", "String"), ("args", "Any")],
            return_type: "Any",
            doc: "Run a Python script or module entry through the local Python runtime",
        },
        BuiltinFn {
            name: "c_compile",
            params: vec![("cwd", "String"), ("source", "String"), ("output", "String"), ("args", "Any")],
            return_type: "Any",
            doc: "Compile a C source into a shared library through the configured C compiler",
        },
        BuiltinFn {
            name: "c_load",
            params: vec![("library", "String")],
            return_type: "Any",
            doc: "Validate and describe a C shared library path",
        },
        BuiltinFn {
            name: "c_call",
            params: vec![("library", "String"), ("symbol", "String"), ("signature", "String"), ("args", "Any")],
            return_type: "Any",
            doc: "Call a simple C ABI symbol using an explicit scalar signature",
        },
        BuiltinFn {
            name: "ts_import",
            params: vec![("cwd", "String"), ("input", "String"), ("output", "String"), ("args", "Any")],
            return_type: "Any",
            doc: "Run Kain's TypeScript importer from Kain",
        },
        BuiltinFn {
            name: "ts_compile",
            params: vec![("cwd", "String"), ("command", "String"), ("args", "Any")],
            return_type: "Any",
            doc: "Run a configured TypeScript toolchain command",
        },
    ] {
        stdlib.functions.insert(builtin.name.to_string(), builtin);
    }
}

fn register_codebase_env(env: &mut Env) {
    if env
        .get_extension_state::<CodebaseConfig>(EXTENSION_KEY)
        .is_none()
    {
        env.set_extension_state(EXTENSION_KEY, Arc::new(CodebaseConfig::default()));
    }
    env.register_native_fn("codebase_find_root", builtin_codebase_find_root);
    env.register_native_fn("codebase_inspect", builtin_codebase_inspect);
    env.register_native_fn("codebase_read", builtin_codebase_read);
    env.register_native_fn("codebase_write", builtin_codebase_write);
    env.register_native_fn("codebase_delete", builtin_codebase_delete);
    env.register_native_fn("codebase_scan", builtin_codebase_scan);
    env.register_native_fn("codebase_read_json", builtin_codebase_read_json);
    env.register_native_fn("codebase_write_json", builtin_codebase_write_json);
    env.register_native_fn("codebase_read_toml", builtin_codebase_read_toml);
    env.register_native_fn("codebase_write_toml", builtin_codebase_write_toml);
    env.register_native_fn("codebase_hash", builtin_codebase_hash);
    env.register_native_fn("codebase_run", builtin_codebase_run);
    env.register_native_fn("cargo_workspace", builtin_cargo_workspace);
    env.register_native_fn("cargo_run", builtin_cargo_run);
    env.register_native_fn("cargo_import_crate", builtin_cargo_import_crate);
    env.register_native_fn("python_import", builtin_python_import);
    env.register_native_fn("python_call", builtin_python_call);
    env.register_native_fn("python_run", builtin_python_run);
    env.register_native_fn("c_compile", builtin_c_compile);
    env.register_native_fn("c_load", builtin_c_load);
    env.register_native_fn("c_call", builtin_c_call);
    env.register_native_fn("ts_import", builtin_ts_import);
    env.register_native_fn("ts_compile", builtin_ts_compile);
}

fn builtin_codebase_find_root(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let path = expect_string_arg(&args, 0, "codebase_find_root")?;
    Ok(Value::String(
        discover_workspace_root(path)?.display().to_string(),
    ))
}

fn builtin_codebase_inspect(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let path = expect_string_arg(&args, 0, "codebase_inspect")?;
    json_to_value(
        &serde_json::to_value(inspect_workspace(path)?).map_err(|err| {
            KainError::runtime(format!("Failed to serialize codebase inspection: {err}"))
        })?,
    )
}

fn builtin_codebase_read(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let path = expect_string_arg(&args, 0, "codebase_read")?;
    Ok(Value::String(read_text(path)?))
}

fn builtin_codebase_write(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let path = expect_string_arg(&args, 0, "codebase_write")?;
    let content = expect_string_arg(&args, 1, "codebase_write")?;
    write_text(path, &content)?;
    Ok(Value::Unit)
}

fn builtin_codebase_delete(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let path = expect_string_arg(&args, 0, "codebase_delete")?;
    delete_path(path)?;
    Ok(Value::Unit)
}

fn builtin_codebase_scan(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let path = expect_string_arg(&args, 0, "codebase_scan")?;
    json_to_value(
        &serde_json::to_value(scan_path(path)?)
            .map_err(|err| KainError::runtime(format!("Failed to serialize scan result: {err}")))?,
    )
}

fn builtin_codebase_read_json(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let path = expect_string_arg(&args, 0, "codebase_read_json")?;
    json_to_value(&read_json_file(path)?)
}

fn builtin_codebase_write_json(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let path = expect_string_arg(&args, 0, "codebase_write_json")?;
    let value = args
        .get(1)
        .ok_or_else(|| KainError::runtime("codebase_write_json expects value"))?;
    write_json_file(path, &value_to_json(value)?)?;
    Ok(Value::Unit)
}

fn builtin_codebase_read_toml(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let path = expect_string_arg(&args, 0, "codebase_read_toml")?;
    json_to_value(&read_toml_file(path)?)
}

fn builtin_codebase_write_toml(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let path = expect_string_arg(&args, 0, "codebase_write_toml")?;
    let value = args
        .get(1)
        .ok_or_else(|| KainError::runtime("codebase_write_toml expects value"))?;
    write_toml_file(path, &value_to_json(value)?)?;
    Ok(Value::Unit)
}

fn builtin_codebase_hash(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let path = expect_string_arg(&args, 0, "codebase_hash")?;
    Ok(Value::String(hash_file(path)?))
}

fn builtin_codebase_run(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let cwd = expect_string_arg(&args, 0, "codebase_run")?;
    let command = expect_string_arg(&args, 1, "codebase_run")?;
    let command_args = args_to_string_vec(args.get(2), "codebase_run")?;
    json_to_value(
        &serde_json::to_value(run_command(cwd, &command, &command_args)?).map_err(|err| {
            KainError::runtime(format!("Failed to serialize command result: {err}"))
        })?,
    )
}

fn builtin_cargo_workspace(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let path = expect_string_arg(&args, 0, "cargo_workspace")?;
    json_to_value(&cargo_workspace(path)?)
}

fn builtin_cargo_run(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let cwd = expect_string_arg(&args, 0, "cargo_run")?;
    let subcommand = expect_string_arg(&args, 1, "cargo_run")?;
    let command_args = args_to_string_vec(args.get(2), "cargo_run")?;
    json_to_value(
        &serde_json::to_value(cargo_run(cwd, &subcommand, &command_args)?).map_err(|err| {
            KainError::runtime(format!("Failed to serialize Cargo result: {err}"))
        })?,
    )
}

fn builtin_cargo_import_crate(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let cwd = expect_string_arg(&args, 0, "cargo_import_crate")?;
    let crate_name = expect_string_arg(&args, 1, "cargo_import_crate")?;
    let command_args = args_to_string_vec(args.get(2), "cargo_import_crate")?;
    json_to_value(
        &serde_json::to_value(cargo_import_crate(cwd, &crate_name, &command_args)?).map_err(
            |err| KainError::runtime(format!("Failed to serialize Cargo import result: {err}")),
        )?,
    )
}

fn builtin_python_import(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let cwd = expect_string_arg(&args, 0, "python_import")?;
    let module = expect_string_arg(&args, 1, "python_import")?;
    json_to_value(&python_import(cwd, &module)?)
}

fn builtin_python_call(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let cwd = expect_string_arg(&args, 0, "python_call")?;
    let module = expect_string_arg(&args, 1, "python_call")?;
    let function = expect_string_arg(&args, 2, "python_call")?;
    let call_args = args
        .get(3)
        .map(value_to_json)
        .transpose()?
        .unwrap_or_else(|| JsonValue::Array(Vec::new()));
    json_to_value(&python_call(cwd, &module, &function, &call_args)?)
}

fn builtin_python_run(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let cwd = expect_string_arg(&args, 0, "python_run")?;
    let script = expect_string_arg(&args, 1, "python_run")?;
    let command_args = args_to_string_vec(args.get(2), "python_run")?;
    json_to_value(
        &serde_json::to_value(python_run(cwd, &script, &command_args)?).map_err(|err| {
            KainError::runtime(format!("Failed to serialize Python result: {err}"))
        })?,
    )
}

fn builtin_c_compile(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let cwd = expect_string_arg(&args, 0, "c_compile")?;
    let source = expect_string_arg(&args, 1, "c_compile")?;
    let output = expect_string_arg(&args, 2, "c_compile")?;
    let command_args = args_to_string_vec(args.get(3), "c_compile")?;
    json_to_value(
        &serde_json::to_value(compile_c_shared(cwd, &source, &output, &command_args)?).map_err(
            |err| KainError::runtime(format!("Failed to serialize C compile result: {err}")),
        )?,
    )
}

fn builtin_c_load(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let library = expect_string_arg(&args, 0, "c_load")?;
    json_to_value(&load_c_library(library)?)
}

fn builtin_c_call(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let library = expect_string_arg(&args, 0, "c_call")?;
    let symbol = expect_string_arg(&args, 1, "c_call")?;
    let signature = expect_string_arg(&args, 2, "c_call")?;
    let call_args = args
        .get(3)
        .map(value_to_json)
        .transpose()?
        .unwrap_or_else(|| JsonValue::Array(Vec::new()));
    json_to_value(&call_c_symbol(library, &symbol, &signature, &call_args)?)
}

fn builtin_ts_import(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let cwd = expect_string_arg(&args, 0, "ts_import")?;
    let input = expect_string_arg(&args, 1, "ts_import")?;
    let output = expect_string_arg(&args, 2, "ts_import")?;
    let command_args = args_to_string_vec(args.get(3), "ts_import")?;
    json_to_value(
        &serde_json::to_value(ts_import(cwd, &input, &output, &command_args)?).map_err(|err| {
            KainError::runtime(format!("Failed to serialize TS import result: {err}"))
        })?,
    )
}

fn builtin_ts_compile(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let cwd = expect_string_arg(&args, 0, "ts_compile")?;
    let command = expect_string_arg(&args, 1, "ts_compile")?;
    let command_args = args_to_string_vec(args.get(2), "ts_compile")?;
    json_to_value(
        &serde_json::to_value(ts_compile(cwd, &command, &command_args)?).map_err(|err| {
            KainError::runtime(format!("Failed to serialize TS compile result: {err}"))
        })?,
    )
}

fn scan_path_inner(
    root: &Path,
    path: &Path,
    entries: &mut Vec<CodebaseFileEntry>,
    limit: usize,
) -> KainResult<()> {
    if entries.len() >= limit {
        return Ok(());
    }
    for entry in kfs::read_dir_entries(path).map_err(fs_to_kain_error)? {
        let path = entry.path;
        let name = entry.file_name;
        if DEFAULT_SCAN_IGNORES.iter().any(|ignored| *ignored == name) {
            continue;
        }
        let relative_path = path.strip_prefix(root).unwrap_or(&path).to_path_buf();
        entries.push(CodebaseFileEntry {
            path: path.clone(),
            relative_path,
            kind: entry.file_type.as_str().to_string(),
            byte_len: if entry.file_type == FsFileType::File {
                entry.metadata.len
            } else {
                0
            },
            extension: path
                .extension()
                .and_then(|value| value.to_str())
                .map(ToString::to_string),
        });
        if entry.file_type == FsFileType::Directory {
            scan_path_inner(root, &path, entries, limit)?;
        }
        if entries.len() >= limit {
            break;
        }
    }
    Ok(())
}

fn parse_successful_json_command(name: &str, result: CodebaseRunResult) -> KainResult<JsonValue> {
    if !result.success {
        return Err(KainError::runtime(format!(
            "{name} failed with status {:?}: {}",
            result.status, result.stderr
        )));
    }
    serde_json::from_str(result.stdout.trim())
        .map_err(|err| KainError::runtime(format!("{name} returned non-JSON output: {err}")))
}

fn normalize_signature(signature: &str) -> String {
    signature
        .chars()
        .filter(|ch| !ch.is_whitespace())
        .collect::<String>()
        .to_ascii_lowercase()
}

fn c_symbol_error(err: libloading::Error) -> KainError {
    KainError::runtime(format!("Failed to resolve C symbol: {err}"))
}

fn fs_to_kain_error(error: kain_fs::FsError) -> KainError {
    KainError::runtime(format!("Filesystem error: {error}"))
}

fn json_i64_arg(args: &[JsonValue], index: usize, name: &str) -> KainResult<i64> {
    args.get(index)
        .and_then(JsonValue::as_i64)
        .ok_or_else(|| KainError::runtime(format!("{name} expected i64 arg {}", index + 1)))
}

fn json_f64_arg(args: &[JsonValue], index: usize, name: &str) -> KainResult<f64> {
    args.get(index)
        .and_then(JsonValue::as_f64)
        .ok_or_else(|| KainError::runtime(format!("{name} expected f64 arg {}", index + 1)))
}

fn normalize_path_for_display(path: &Path) -> KainResult<PathBuf> {
    if path.exists() {
        kfs::canonicalize_path(path)
            .map(PathBuf::from)
            .map_err(fs_to_kain_error)
    } else if let Some(parent) = path.parent() {
        Ok(existing_directory_anchor(parent)?.join(
            path.file_name()
                .unwrap_or_default()
                .to_string_lossy()
                .to_string(),
        ))
    } else {
        Ok(path.to_path_buf())
    }
}

fn existing_directory_anchor(path: &Path) -> KainResult<PathBuf> {
    let candidate = if path.exists() && path.is_file() {
        path.parent().unwrap_or(Path::new(".")).to_path_buf()
    } else {
        path.to_path_buf()
    };
    if candidate.exists() {
        return kfs::canonicalize_path(candidate)
            .map(PathBuf::from)
            .map_err(fs_to_kain_error);
    }
    let mut current = candidate.as_path();
    loop {
        if current.exists() {
            return kfs::canonicalize_path(current)
                .map(PathBuf::from)
                .map_err(fs_to_kain_error);
        }
        let Some(parent) = current.parent() else {
            return std::env::current_dir().map_err(KainError::Io);
        };
        current = parent;
    }
}

fn expect_string_arg(args: &[Value], index: usize, name: &str) -> KainResult<String> {
    match args.get(index) {
        Some(Value::String(value)) => Ok(value.clone()),
        Some(other) => Err(KainError::runtime(format!(
            "{name} expected argument {} to be String, got {other}",
            index + 1
        ))),
        None => Err(KainError::runtime(format!(
            "{name} expected argument {}",
            index + 1
        ))),
    }
}

fn args_to_string_vec(value: Option<&Value>, name: &str) -> KainResult<Vec<String>> {
    match value {
        None | Some(Value::None) | Some(Value::Unit) => Ok(Vec::new()),
        Some(Value::String(value)) => Ok(vec![value.clone()]),
        Some(Value::Array(items)) => {
            let items = items.read().unwrap();
            items
                .iter()
                .map(|item| match item {
                    Value::String(value) => Ok(value.clone()),
                    Value::Int(value) => Ok(value.to_string()),
                    Value::Float(value) => Ok(value.to_string()),
                    Value::Bool(value) => Ok(value.to_string()),
                    other => Err(KainError::runtime(format!(
                        "{name} expected args array to contain strings, got {other}"
                    ))),
                })
                .collect()
        }
        Some(other) => Err(KainError::runtime(format!(
            "{name} expected args to be Array<String>, got {other}"
        ))),
    }
}

fn value_to_json(value: &Value) -> KainResult<JsonValue> {
    match value {
        Value::Unit | Value::None => Ok(JsonValue::Null),
        Value::Bool(value) => Ok(JsonValue::Bool(*value)),
        Value::Int(value) => Ok(json!(value)),
        Value::Float(value) => Ok(json!(value)),
        Value::String(value) => Ok(JsonValue::String(value.clone())),
        Value::Array(items) => {
            let items = items.read().unwrap();
            let mut values = Vec::with_capacity(items.len());
            for item in items.iter() {
                values.push(value_to_json(item)?);
            }
            Ok(JsonValue::Array(values))
        }
        Value::Tuple(items) => {
            let mut values = Vec::with_capacity(items.len());
            for item in items {
                values.push(value_to_json(item)?);
            }
            Ok(JsonValue::Array(values))
        }
        Value::Struct(_, fields) => {
            let fields = fields.read().unwrap();
            let mut object = Map::new();
            for (key, value) in fields.iter() {
                object.insert(key.clone(), value_to_json(value)?);
            }
            Ok(JsonValue::Object(object))
        }
        other => Ok(JsonValue::String(other.to_string())),
    }
}

fn json_to_value(value: &JsonValue) -> KainResult<Value> {
    match value {
        JsonValue::Null => Ok(Value::None),
        JsonValue::Bool(value) => Ok(Value::Bool(*value)),
        JsonValue::Number(number) => {
            if let Some(value) = number.as_i64() {
                Ok(Value::Int(value))
            } else if let Some(value) = number.as_f64() {
                Ok(Value::Float(value))
            } else {
                Err(KainError::runtime(format!(
                    "Unsupported numeric value in codebase projection: {number}"
                )))
            }
        }
        JsonValue::String(value) => Ok(Value::String(value.clone())),
        JsonValue::Array(items) => {
            let mut values = Vec::with_capacity(items.len());
            for item in items {
                values.push(json_to_value(item)?);
            }
            Ok(Value::Array(Arc::new(RwLock::new(values))))
        }
        JsonValue::Object(object) => {
            let mut fields = HashMap::new();
            for (key, value) in object {
                fields.insert(key.clone(), json_to_value(value)?);
            }
            Ok(Value::Struct(
                "CodebaseObject".to_string(),
                Arc::new(RwLock::new(fields)),
            ))
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use kain_core::diagnostics::SpanMapper;
    use kain_core::lexer::Lexer;
    use kain_core::parser::Parser;
    use kain_core::runtime::{interpret, Value};
    use kain_core::types;
    use kain_core::CompileTarget;
    use tempfile::tempdir;

    #[test]
    fn read_write_delete_scan_and_hash_are_trusted_local_operations() {
        let dir = tempdir().unwrap();
        let root = dir.path();
        write_text(root.join("KAIN.toml"), "[package]\nname = \"demo\"\n").unwrap();
        write_text(root.join("src/main.kn"), "fn main(): print(\"hi\")\n").unwrap();

        assert_eq!(
            discover_workspace_root(root.join("src/main.kn")).unwrap(),
            PathBuf::from(kfs::canonicalize_path(root).unwrap())
        );
        assert!(read_text(root.join("src/main.kn"))
            .unwrap()
            .contains("main"));

        let entries = scan_path(root).unwrap();
        assert!(entries
            .iter()
            .any(|entry| entry.relative_path == PathBuf::from("src")));
        assert!(entries
            .iter()
            .any(|entry| entry.relative_path == PathBuf::from("src/main.kn")));

        let digest = hash_file(root.join("src/main.kn")).unwrap();
        assert_eq!(digest.len(), 64);

        delete_path(root.join("src")).unwrap();
        assert!(!root.join("src").exists());
    }

    #[test]
    fn json_and_toml_round_trip_through_structured_values() {
        let dir = tempdir().unwrap();
        let json_path = dir.path().join("settings.json");
        let toml_path = dir.path().join("runtime.toml");

        write_json_file(&json_path, &json!({ "enabled": true, "count": 3 })).unwrap();
        let json_value = read_json_file(&json_path).unwrap();
        assert_eq!(json_value["enabled"], JsonValue::Bool(true));

        write_toml_file(&toml_path, &json!({ "runtime": { "name": "fabric" } })).unwrap();
        let toml_value = read_toml_file(&toml_path).unwrap();
        assert_eq!(
            toml_value["runtime"]["name"],
            JsonValue::String("fabric".to_string())
        );
    }

    #[test]
    fn command_run_captures_status_stdout_and_stderr() {
        let dir = tempdir().unwrap();
        let result = run_command(dir.path(), "rustc", &["--version".to_string()]).unwrap();
        assert!(result.success);
        assert!(result.stdout.contains("rustc"));
        assert_eq!(result.cwd, dir.path());
    }

    #[test]
    fn codebase_stdlib_functions_execute_from_kain_source() {
        let dir = tempdir().unwrap();
        let authored_path = dir.path().join("authored-through-kain.txt");
        let escaped_path = escape_kain_string(&authored_path.display().to_string());
        let source = format!(
            r#"
fn main() -> Any:
    codebase_write("{escaped_path}", "trusted-local-kain-write")
    return codebase_read("{escaped_path}")
"#
        );

        let value = interpret_codebase_source(&source);
        match value {
            Value::String(value) => assert_eq!(value, "trusted-local-kain-write"),
            other => panic!("expected codebase_read string output, got {other:?}"),
        }
        assert_eq!(
            read_text(&authored_path).unwrap(),
            "trusted-local-kain-write"
        );
    }

    fn interpret_codebase_source(source: &str) -> Value {
        register();
        let stdlib = kain_core::stdlib::load_stdlib_for_target(CompileTarget::Interpret);
        let full_source = format!("{stdlib}\n{source}");
        let tokens = Lexer::new(&full_source).tokenize().unwrap();
        let span_mapper = SpanMapper::new(&full_source);
        let mut ast = Parser::new(&tokens, &span_mapper, "<codebase-test>")
            .parse()
            .unwrap();
        kain_core::comptime::eval_program(&mut ast).unwrap();
        let typed = types::check(&ast, &span_mapper, "<codebase-test>").unwrap();
        interpret(&typed).unwrap()
    }

    fn escape_kain_string(value: &str) -> String {
        value.replace('\\', "\\\\").replace('"', "\\\"")
    }
}
