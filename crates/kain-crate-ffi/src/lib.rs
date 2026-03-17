mod config;
mod extract;
mod generate;
mod model;
mod resolve;

pub use config::{RustFfiConfig, RustFfiPathCrate, RustFfiRegistryCrate};
pub use generate::{bridge_crate_name, BRIDGE_FORMAT_VERSION, BRIDGE_SYMBOL_NAME};
pub use model::{
    ArtifactMode, BindingReport, BindingReportEntry, ImportCrateOptions, ImportCrateOutput,
    ItemKind, ItemStatus, PrepareContext, ResolutionKind, ResolvedCrate,
};

use extract::extract_binding_bundle;
use generate::write_generated_artifacts;
use kain_core::error::KainError;
use kain_core::runtime::{register_env_extension, Env};
use kain_core::CompileTarget;
use libloading::{Library, Symbol};
use once_cell::sync::Lazy;
use resolve::{
    build_cache_hash, build_cache_inputs, lib_filename, resolve_crate, target_directory_name,
};
use std::collections::{BTreeMap, BTreeSet};
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;
use std::sync::{Once, RwLock};

static REGISTER_EXTENSION_ONCE: Once = Once::new();
static LOADED_BRIDGES: Lazy<RwLock<BTreeMap<PathBuf, LoadedBridge>>> =
    Lazy::new(|| RwLock::new(BTreeMap::new()));

type RegisterBridgeFn = unsafe extern "C" fn(*mut Env);

struct LoadedBridge {
    _library: Library,
    register: RegisterBridgeFn,
}

pub fn register() {
    REGISTER_EXTENSION_ONCE.call_once(|| {
        register_env_extension("kain_crate_ffi", apply_loaded_bridges);
    });
}

pub fn import_crate(
    crate_name: &str,
    options: &ImportCrateOptions,
    prepare: &PrepareContext,
) -> Result<ImportCrateOutput, KainError> {
    register();
    let (resolved, manifest_context) = resolve_crate(crate_name, options, prepare)?;
    let bundle = extract_binding_bundle(&resolved)?;
    let source_files = bundle
        .source_fingerprints
        .iter()
        .map(|value| PathBuf::from(&value.path))
        .collect::<Vec<_>>();
    let cache_inputs = build_cache_inputs(&source_files)?;
    let hash = build_cache_hash(
        &resolved,
        &cache_inputs.source_file_hashes,
        &cache_inputs.target_triple,
        &cache_inputs.rustc_version,
        env!("CARGO_PKG_VERSION"),
        BRIDGE_FORMAT_VERSION,
    );
    let cache_dir = default_cache_root(prepare)
        .join("crate_ffi")
        .join(hash);
    fs::create_dir_all(&cache_dir).map_err(KainError::Io)?;

    let (artifacts, mut output) =
        write_generated_artifacts(&resolved, &bundle, &cache_dir, options.output_dir.as_deref())?;
    output.config_root = manifest_context.root_dir.clone();
    output.rust_ffi_config = manifest_context.config.clone();

    if let Some(report_json_path) = options.report_json.as_ref() {
        if let Some(parent) = report_json_path.parent() {
            fs::create_dir_all(parent).map_err(KainError::Io)?;
        }
        let report_json = serde_json::to_string_pretty(&artifacts.report).map_err(|err| {
            KainError::runtime(format!("Failed to serialize crate FFI report override: {err}"))
        })?;
        fs::write(report_json_path, report_json).map_err(KainError::Io)?;
    }

    if options.mode.wants_live() {
        let (dylib_path, cache_hit) = ensure_bridge_library(&output, &resolved)?;
        ensure_bridge_loaded(&dylib_path)?;
        output.dylib_path = Some(dylib_path);
        output.cache_hit = cache_hit;
    }

    Ok(output)
}

pub fn augment_source_for_runtime(
    source: &str,
    target: CompileTarget,
    prepare: &PrepareContext,
) -> Result<String, KainError> {
    let imports = detect_rust_crate_imports(source);
    if imports.is_empty() {
        return Ok(source.to_string());
    }
    if !matches!(target, CompileTarget::Interpret | CompileTarget::Test) {
        return Err(KainError::runtime(
            "Rust crate FFI is only available in host-backed Kain execution lanes for now",
        ));
    }

    let mut sections = Vec::new();
    let mut seen = BTreeSet::new();
    for import_name in imports {
        if !seen.insert(import_name.clone()) {
            continue;
        }
        let options = ImportCrateOptions {
            mode: ArtifactMode::Both,
            ..ImportCrateOptions::default()
        };
        let output = import_crate(&import_name, &options, prepare)?;
        sections.push(output.canonical_module_source);
    }
    sections.push(source.to_string());
    Ok(sections.join("\n"))
}

pub fn detect_rust_crate_imports(source: &str) -> Vec<String> {
    static IMPORT_REGEX: Lazy<regex::Regex> = Lazy::new(|| {
        regex::Regex::new(r"(?m)^\s*use\s+rust(?:::|/)([A-Za-z_][A-Za-z0-9_]*)").expect("regex")
    });
    let mut seen = BTreeSet::new();
    let mut imports = Vec::new();
    for captures in IMPORT_REGEX.captures_iter(source) {
        if let Some(value) = captures.get(1) {
            let crate_name = value.as_str().to_string();
            if seen.insert(crate_name.clone()) {
                imports.push(crate_name);
            }
        }
    }
    imports
}

fn apply_loaded_bridges(env: &mut Env) {
    let registry = LOADED_BRIDGES.read().expect("crate ffi bridge registry read");
    for bridge in registry.values() {
        unsafe { (bridge.register)(env as *mut Env) };
    }
}

fn ensure_bridge_loaded(dylib_path: &Path) -> Result<(), KainError> {
    register();
    let canonical_path = fs::canonicalize(dylib_path).unwrap_or_else(|_| dylib_path.to_path_buf());
    if LOADED_BRIDGES
        .read()
        .expect("crate ffi bridge registry read")
        .contains_key(&canonical_path)
    {
        return Ok(());
    }

    let library = unsafe { Library::new(&canonical_path) }.map_err(|err| {
        KainError::runtime(format!(
            "Failed to load Rust crate FFI bridge '{}': {err}",
            canonical_path.display()
        ))
    })?;
    let register = unsafe {
        let symbol: Symbol<RegisterBridgeFn> =
            library.get(BRIDGE_SYMBOL_NAME).map_err(|err| {
                KainError::runtime(format!(
                    "Bridge '{}' is missing symbol '{}': {err}",
                    canonical_path.display(),
                    String::from_utf8_lossy(BRIDGE_SYMBOL_NAME)
                ))
            })?;
        *symbol
    };
    LOADED_BRIDGES
        .write()
        .expect("crate ffi bridge registry write")
        .insert(
            canonical_path,
            LoadedBridge {
                _library: library,
                register,
            },
        );
    Ok(())
}

fn ensure_bridge_library(
    output: &ImportCrateOutput,
    resolved: &ResolvedCrate,
) -> Result<(PathBuf, bool), KainError> {
    let bridge_name = bridge_crate_name(&resolved.import_name);
    let target_dir = output.cache_dir.join("bridge").join("target");
    let dylib_path = target_dir
        .join(target_directory_name())
        .join(lib_filename(&bridge_name));
    if dylib_path.exists() {
        return Ok((dylib_path, true));
    }

    let status = Command::new("cargo")
        .arg("build")
        .arg("--manifest-path")
        .arg(&output.bridge_manifest_path)
        .env("CARGO_TARGET_DIR", &target_dir)
        .status()
        .map_err(|err| {
            KainError::runtime(format!(
                "Failed to run cargo build for Rust crate FFI bridge '{}': {err}",
                output.bridge_manifest_path.display()
            ))
        })?;
    if !status.success() {
        return Err(KainError::runtime(format!(
            "Cargo build failed for Rust crate FFI bridge '{}'",
            output.bridge_manifest_path.display()
        )));
    }
    if !dylib_path.exists() {
        return Err(KainError::runtime(format!(
            "Rust crate FFI bridge build succeeded but '{}' was not produced",
            dylib_path.display()
        )));
    }
    Ok((dylib_path, false))
}

fn default_cache_root(prepare: &PrepareContext) -> PathBuf {
    if let Some(dir) = prepare.current_dir.as_ref() {
        return dir.join(".kain").join("cache");
    }
    std::env::current_dir()
        .unwrap_or_else(|_| PathBuf::from("."))
        .join(".kain")
        .join("cache")
}

#[cfg(test)]
mod tests {
    use super::*;
    use tempfile::TempDir;

    #[test]
    fn detects_rust_imports() {
        let source = "use std::python::bridge\nuse rust::browser\nuse rust::glam\n";
        assert_eq!(
            detect_rust_crate_imports(source),
            vec!["browser".to_string(), "glam".to_string()]
        );
    }

    #[test]
    fn imports_local_crate_path_and_generates_bindings() {
        let temp = TempDir::new().expect("temp dir");
        let crate_dir = temp.path().join("sample_ffi");
        fs::create_dir_all(crate_dir.join("src")).expect("create crate");
        fs::write(
            crate_dir.join("Cargo.toml"),
            "[package]\nname = \"sample_ffi\"\nversion = \"0.1.0\"\nedition = \"2021\"\n",
        )
        .expect("write Cargo.toml");
        fs::write(
            crate_dir.join("src").join("lib.rs"),
            "pub fn add(a: i64, b: i64) -> i64 { a + b }\npub fn label() -> &'static str { \"sample\" }\n",
        )
        .expect("write lib.rs");

        let output = import_crate(
            "sample_ffi",
            &ImportCrateOptions {
                crate_path: Some(crate_dir.clone()),
                mode: ArtifactMode::Generate,
                ..ImportCrateOptions::default()
            },
            &PrepareContext {
                current_dir: Some(temp.path().to_path_buf()),
                manifest_path: None,
            },
        )
        .expect("import crate");

        assert!(output.canonical_module_source.contains("mod rust:"));
        assert!(output.canonical_module_source.contains("fn add(a: Int, b: Int) -> Int:"));
        assert!(output.canonical_module_source.contains("fn rust_sample_ffi_add(a: Int, b: Int) -> Int:"));
        assert!(output.prelude_source.contains("use rust::sample_ffi::rust_sample_ffi_add"));
        assert!(output.report_json_path.exists());
    }

    #[test]
    fn live_import_reuses_cached_bridge_for_local_crate_path() {
        let temp = TempDir::new().expect("temp dir");
        let crate_dir = temp.path().join("sample_live_ffi");
        fs::create_dir_all(crate_dir.join("src")).expect("create crate");
        fs::write(
            crate_dir.join("Cargo.toml"),
            "[package]\nname = \"sample_live_ffi\"\nversion = \"0.1.0\"\nedition = \"2021\"\n",
        )
        .expect("write Cargo.toml");
        fs::write(
            crate_dir.join("src").join("lib.rs"),
            "pub fn add(a: i64, b: i64) -> i64 { a + b }\n",
        )
        .expect("write lib.rs");

        let prepare = PrepareContext {
            current_dir: Some(temp.path().to_path_buf()),
            manifest_path: None,
        };
        let options = ImportCrateOptions {
            crate_path: Some(crate_dir.clone()),
            mode: ArtifactMode::Both,
            ..ImportCrateOptions::default()
        };

        let first = import_crate("sample_live_ffi", &options, &prepare).expect("first import");
        assert!(
            first.dylib_path
                .as_ref()
                .is_some_and(|value| value.exists()),
            "first live import should build a bridge library"
        );
        assert!(!first.cache_hit, "first live import should build the bridge");

        let second = import_crate("sample_live_ffi", &options, &prepare).expect("second import");
        assert!(
            second
                .dylib_path
                .as_ref()
                .is_some_and(|value| value.exists()),
            "second live import should reuse the bridge library"
        );
        assert!(second.cache_hit, "second live import should reuse the cached bridge");
    }
}
