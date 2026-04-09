mod config;
mod extract;
mod generate;
mod model;

pub use config::{CFfiConfig, CLibraryConfig};
pub use generate::{bridge_crate_name, BRIDGE_FORMAT_VERSION, BRIDGE_SYMBOL_NAME};
pub use model::{
    ArtifactMode, BindingManifest, BindingReport, BindingReportEntry, HostBridgeModuleDescriptor,
    HostBridgeServiceDescriptor, ImportCOptions, ImportCOutput, ItemKind, ItemStatus,
    PackagedBridgeBinaryArtifact, PackagedBridgeImport, PackagedBridgeManifest,
    PackagedBridgeSymbolDescriptor, PrepareContext, ResolvedCLibrary,
};

use extract::extract_binding_bundle;
use generate::write_generated_artifacts;
use kain_core::error::KainError;
use kain_core::runtime::{register_env_extension, Env};
use kain_core::CompileTarget;
use libloading::{Library, Symbol};
use once_cell::sync::Lazy;
use regex::Regex;
use sha2::{Digest, Sha256};
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
        register_env_extension("kain_c_ffi", apply_loaded_bridges);
    });
}

pub fn import_libraries_for_source(
    source: &str,
    options: &ImportCOptions,
    prepare: &PrepareContext,
) -> Result<Vec<ImportCOutput>, KainError> {
    let imports = detect_c_library_imports(source);
    let mut outputs = Vec::new();
    for import_name in imports {
        outputs.push(import_library(&import_name, options, prepare)?);
    }
    Ok(outputs)
}

pub fn load_prebuilt_bridge(dylib_path: &Path) -> Result<(), KainError> {
    ensure_bridge_loaded(dylib_path)
}

pub fn shared_library_env_var(import_name: &str) -> String {
    let mut suffix = String::with_capacity(import_name.len());
    for ch in import_name.chars() {
        match ch {
            'A'..='Z' => suffix.push(ch),
            'a'..='z' => suffix.push(ch.to_ascii_uppercase()),
            '0'..='9' => suffix.push(ch),
            _ => suffix.push('_'),
        }
    }
    format!("KAIN_C_FFI_SHARED_LIB_{suffix}")
}

pub fn load_packaged_bridges_from_manifest(manifest_path: &Path) -> Result<usize, KainError> {
    register();
    let manifest_source = fs::read_to_string(manifest_path).map_err(|err| {
        KainError::runtime(format!(
            "Failed to read packaged C FFI manifest '{}': {err}",
            manifest_path.display()
        ))
    })?;
    let manifest: PackagedBridgeManifest =
        serde_json::from_str(&manifest_source).map_err(|err| {
            KainError::runtime(format!(
                "Failed to parse packaged C FFI manifest '{}': {err}",
                manifest_path.display()
            ))
        })?;
    let manifest_dir = manifest_path.parent().unwrap_or_else(|| Path::new("."));
    let mut loaded = 0usize;

    for import in manifest.imports {
        if let Some(shared_library) = import.shared_library.as_ref() {
            let shared_path = resolve_packaged_bridge_artifact(manifest_dir, shared_library)
                .ok_or_else(|| {
                    KainError::runtime(format!(
                        "Failed to resolve packaged shared library '{}' for C import '{}'",
                        shared_library.file_name, import.import_name
                    ))
                })?;
            std::env::set_var(shared_library_env_var(&import.import_name), &shared_path);
        }

        let bridge_path = resolve_packaged_bridge_artifact(manifest_dir, &import.bridge_library)
            .ok_or_else(|| {
                KainError::runtime(format!(
                    "Failed to resolve packaged bridge library '{}' for C import '{}'",
                    import.bridge_library.file_name, import.import_name
                ))
            })?;
        ensure_bridge_loaded(&bridge_path)?;
        loaded += 1;
    }

    Ok(loaded)
}

pub fn import_library(
    import_name: &str,
    options: &ImportCOptions,
    prepare: &PrepareContext,
) -> Result<ImportCOutput, KainError> {
    register();
    let (resolved, manifest_context) = resolve_library(import_name, prepare)?;
    let bundle = extract_binding_bundle(&resolved)?;
    let hash = build_cache_hash(
        &resolved,
        &bundle.source_fingerprints,
        BRIDGE_FORMAT_VERSION,
    );
    let cache_dir = default_cache_root(prepare).join("c_ffi").join(hash);
    fs::create_dir_all(&cache_dir).map_err(KainError::Io)?;

    let (artifacts, mut output) = write_generated_artifacts(
        &resolved,
        &bundle,
        &cache_dir,
        options.output_dir.as_deref(),
    )?;
    output.config_root = manifest_context.root_dir.clone();
    output.c_ffi_config = manifest_context.config.clone();

    if let Some(report_json_path) = options.report_json.as_ref() {
        if let Some(parent) = report_json_path.parent() {
            fs::create_dir_all(parent).map_err(KainError::Io)?;
        }
        let report_json = serde_json::to_string_pretty(&artifacts.report).map_err(|err| {
            KainError::runtime(format!("Failed to serialize C FFI report override: {err}"))
        })?;
        fs::write(report_json_path, report_json).map_err(KainError::Io)?;
    }

    if options.mode.wants_live() {
        let (dylib_path, cache_hit) = ensure_bridge_library(&output)?;
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
    let imports = detect_c_library_imports(source);
    if imports.is_empty() {
        return Ok(source.to_string());
    }

    let mode = artifact_mode_for_target(target).ok_or_else(|| {
        KainError::runtime(
            "C ABI FFI is currently available in Interpret, Test, and Rust/native packaging lanes",
        )
    })?;
    let mut outputs = Vec::with_capacity(imports.len());
    for import_name in imports {
        outputs.push(import_library(
            &import_name,
            &ImportCOptions {
                mode,
                ..ImportCOptions::default()
            },
            prepare,
        )?);
    }

    let mut sections = Vec::new();
    for output in outputs {
        sections.push(output.canonical_module_source);
    }
    sections.push(source.to_string());
    Ok(sections.join("\n"))
}

pub fn detect_c_library_imports(source: &str) -> Vec<String> {
    static IMPORT_REGEX: Lazy<Regex> = Lazy::new(|| {
        Regex::new(r"(?m)^\s*use\s+c(?:::|/)([A-Za-z_][A-Za-z0-9_]*)").expect("regex")
    });
    let mut seen = BTreeSet::new();
    let mut imports = Vec::new();
    for captures in IMPORT_REGEX.captures_iter(source) {
        if let Some(value) = captures.get(1) {
            let import_name = value.as_str().to_string();
            if seen.insert(import_name.clone()) {
                imports.push(import_name);
            }
        }
    }
    imports
}

fn apply_loaded_bridges(env: &mut Env) {
    let registry = LOADED_BRIDGES.read().expect("c ffi bridge registry read");
    for bridge in registry.values() {
        unsafe { (bridge.register)(env as *mut Env) };
    }
}

fn resolve_packaged_bridge_artifact(
    manifest_dir: &Path,
    artifact: &PackagedBridgeBinaryArtifact,
) -> Option<PathBuf> {
    if let Some(current_exe_candidate) = std::env::current_exe()
        .ok()
        .and_then(|exe| exe.parent().map(|dir| dir.join(&artifact.file_name)))
        .filter(|path| path.exists())
    {
        return Some(current_exe_candidate);
    }
    let manifest_candidate = manifest_dir.join(&artifact.file_name);
    if manifest_candidate.exists() {
        return Some(manifest_candidate);
    }
    artifact
        .source_path
        .as_ref()
        .map(PathBuf::from)
        .filter(|path| path.exists())
}

fn artifact_mode_for_target(target: CompileTarget) -> Option<ArtifactMode> {
    match target {
        CompileTarget::Interpret | CompileTarget::Test => Some(ArtifactMode::Both),
        CompileTarget::Rust => Some(ArtifactMode::Generate),
        _ => None,
    }
}

fn ensure_bridge_loaded(dylib_path: &Path) -> Result<(), KainError> {
    register();
    let canonical_path = fs::canonicalize(dylib_path).unwrap_or_else(|_| dylib_path.to_path_buf());
    if LOADED_BRIDGES
        .read()
        .expect("c ffi bridge registry read")
        .contains_key(&canonical_path)
    {
        return Ok(());
    }

    let library = unsafe { Library::new(&canonical_path) }.map_err(|err| {
        KainError::runtime(format!(
            "Failed to load C FFI bridge '{}': {err}",
            canonical_path.display()
        ))
    })?;
    let register = unsafe {
        let symbol: Symbol<RegisterBridgeFn> = library.get(BRIDGE_SYMBOL_NAME).map_err(|err| {
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
        .expect("c ffi bridge registry write")
        .insert(
            canonical_path,
            LoadedBridge {
                _library: library,
                register,
            },
        );
    Ok(())
}

fn ensure_bridge_library(output: &ImportCOutput) -> Result<(PathBuf, bool), KainError> {
    let bridge_name = bridge_crate_name(&output.resolved.import_name);
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
                "Failed to run cargo build for C FFI bridge '{}': {err}",
                output.bridge_manifest_path.display()
            ))
        })?;
    if !status.success() {
        return Err(KainError::runtime(format!(
            "Cargo build failed for C FFI bridge '{}'",
            output.bridge_manifest_path.display()
        )));
    }
    if !dylib_path.exists() {
        return Err(KainError::runtime(format!(
            "C FFI bridge build succeeded but '{}' was not produced",
            dylib_path.display()
        )));
    }
    Ok((dylib_path, false))
}

fn resolve_library(
    import_name: &str,
    prepare: &PrepareContext,
) -> Result<(ResolvedCLibrary, model::ManifestContext), KainError> {
    let start_dir = prepare
        .manifest_path
        .as_ref()
        .and_then(|value| value.parent().map(Path::to_path_buf))
        .or_else(|| prepare.current_dir.clone())
        .unwrap_or_else(|| std::env::current_dir().unwrap_or_else(|_| PathBuf::from(".")));
    let manifest_root = find_kain_manifest_root(&start_dir).ok_or_else(|| {
        KainError::runtime(format!(
            "Could not resolve KAIN.toml for C FFI import '{}'",
            import_name
        ))
    })?;
    let config = load_c_ffi_config(&manifest_root)?.ok_or_else(|| {
        KainError::runtime(format!(
            "KAIN.toml at '{}' is missing [c_ffi] configuration",
            manifest_root.display()
        ))
    })?;
    let library = config
        .libraries
        .iter()
        .find(|value| value.name == import_name)
        .cloned()
        .ok_or_else(|| {
            KainError::runtime(format!(
                "No [c_ffi] library named '{}' found in '{}'",
                import_name,
                manifest_root.join("KAIN.toml").display()
            ))
        })?;

    let header_path = resolve_relative_path(&manifest_root, &library.header);
    if !header_path.exists() {
        return Err(KainError::runtime(format!(
            "C FFI header '{}' does not exist",
            header_path.display()
        )));
    }
    let shared_lib_path = library
        .shared_lib
        .as_ref()
        .map(|value| resolve_relative_path(&manifest_root, value));

    Ok((
        ResolvedCLibrary {
            import_name: import_name.to_string(),
            manifest_root: manifest_root.clone(),
            header_path,
            shared_lib_path,
            config: library,
            global_config: config.clone(),
        },
        model::ManifestContext {
            root_dir: Some(manifest_root),
            config: Some(config),
        },
    ))
}

fn load_c_ffi_config(root: &Path) -> Result<Option<config::CFfiConfig>, KainError> {
    for name in ["KAIN.toml", "kain.toml"] {
        let manifest_path = root.join(name);
        if !manifest_path.exists() {
            continue;
        }
        let source = fs::read_to_string(&manifest_path).map_err(KainError::Io)?;
        let value: toml::Value = toml::from_str(&source).map_err(|err| {
            KainError::runtime(format!(
                "Failed to parse '{}': {err}",
                manifest_path.display()
            ))
        })?;
        if let Some(table) = value.get("c_ffi") {
            let config = table
                .clone()
                .try_into::<config::CFfiConfig>()
                .map_err(|err| {
                    KainError::runtime(format!(
                        "Failed to parse [c_ffi] in '{}': {err}",
                        manifest_path.display()
                    ))
                })?;
            return Ok(Some(config));
        }
        return Ok(None);
    }
    Ok(None)
}

fn find_kain_manifest_root(start_dir: &Path) -> Option<PathBuf> {
    let mut current = Some(start_dir);
    while let Some(dir) = current {
        if ["KAIN.toml", "kain.toml"]
            .iter()
            .any(|name| dir.join(name).exists())
        {
            return Some(dir.to_path_buf());
        }
        current = dir.parent();
    }
    None
}

fn resolve_relative_path(root: &Path, path: &Path) -> PathBuf {
    let expanded = expand_platform_dynamic_library_tokens(path);
    if expanded.is_absolute() {
        expanded
    } else {
        root.join(expanded)
    }
}

fn expand_platform_dynamic_library_tokens(path: &Path) -> PathBuf {
    let source = path.to_string_lossy();
    let mut expanded = source.into_owned();
    const TOKEN_PREFIX: &str = "${kain_dynlib:";

    while let Some(start) = expanded.find(TOKEN_PREFIX) {
        let token_end = expanded[start..]
            .find('}')
            .map(|offset| start + offset)
            .unwrap_or(expanded.len());
        if token_end >= expanded.len() {
            break;
        }
        let library_stem = &expanded[start + TOKEN_PREFIX.len()..token_end];
        let replacement = current_platform_dynamic_library_name(library_stem);
        expanded.replace_range(start..=token_end, &replacement);
    }

    PathBuf::from(expanded)
}

fn current_platform_dynamic_library_name(stem: &str) -> String {
    if cfg!(target_os = "windows") {
        format!("{stem}.dll")
    } else if cfg!(target_os = "macos") {
        format!("lib{stem}.dylib")
    } else {
        format!("lib{stem}.so")
    }
}

fn build_cache_hash(
    resolved: &ResolvedCLibrary,
    fingerprints: &[model::FileFingerprint],
    format_version: &str,
) -> String {
    let mut hasher = Sha256::new();
    hasher.update(resolved.import_name.as_bytes());
    hasher.update(resolved.header_path.display().to_string().as_bytes());
    if let Some(path) = &resolved.shared_lib_path {
        hasher.update(path.display().to_string().as_bytes());
    }
    hasher.update(format_version.as_bytes());
    for fingerprint in fingerprints {
        hasher.update(fingerprint.path.as_bytes());
        hasher.update(fingerprint.sha256.as_bytes());
    }
    format!("{:x}", hasher.finalize())
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

fn target_directory_name() -> &'static str {
    "debug"
}

fn lib_filename(crate_name: &str) -> String {
    if cfg!(target_os = "windows") {
        format!("{crate_name}.dll")
    } else if cfg!(target_os = "macos") {
        format!("lib{crate_name}.dylib")
    } else {
        format!("lib{crate_name}.so")
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
    use std::fs;
    use tempfile::TempDir;

    #[test]
    fn detects_c_imports() {
        let source = "use c::beacon_math\nuse c::image_fx\n";
        assert_eq!(
            detect_c_library_imports(source),
            vec!["beacon_math".to_string(), "image_fx".to_string()]
        );
    }

    #[test]
    fn prepare_blocks_non_host_targets_when_c_bridge_is_used() {
        let error = augment_source_for_runtime(
            "use c::beacon_math\nfn main(): return 0\n",
            CompileTarget::Js,
            &PrepareContext::default(),
        )
        .expect_err("c bridge should reject JS codegen target");
        assert!(error.to_string().contains("Interpret, Test, and Rust"));
    }

    #[test]
    fn rust_target_prepares_generated_c_bridge_without_live_loading() {
        let temp = TempDir::new().expect("temp dir");
        let root = temp.path();
        let native_dir = root.join("native");
        fs::create_dir_all(&native_dir).expect("native dir");

        let (header_path, source_path, dll_path) = c_fixture_paths(&native_dir);
        fs::write(
            &header_path,
            "#if defined(_WIN32)\n#define BEACON_EXPORT __declspec(dllexport)\n#else\n#define BEACON_EXPORT\n#endif\nBEACON_EXPORT int beacon_add(int a, int b);\n",
        )
        .expect("header");
        fs::write(
            &source_path,
            "#include \"beacon_math.h\"\nint beacon_add(int a, int b) { return a + b; }\n",
        )
        .expect("source");
        compile_shared_library(&source_path, &dll_path);
        write_c_manifest(root, "beacon_math", &header_path, &dll_path);

        let augmented = augment_source_for_runtime(
            "use c::beacon_math\nfn main() -> Int:\n    return beacon_add(2, 3)\n",
            CompileTarget::Rust,
            &PrepareContext {
                current_dir: Some(root.to_path_buf()),
                manifest_path: Some(root.join("KAIN.toml")),
            },
        )
        .expect("rust target should accept generated c bridge bindings");

        assert!(augmented.contains("mod c:"));
        assert!(augmented.contains("beacon_add"));
    }

    #[test]
    fn packaged_bridge_manifest_loads_prebuilt_bridge_from_copied_sidecars() {
        let temp = TempDir::new().expect("temp dir");
        let root = temp.path();
        let native_dir = root.join("native");
        let package_dir = root.join("package");
        fs::create_dir_all(&native_dir).expect("native dir");
        fs::create_dir_all(&package_dir).expect("package dir");

        let (header_path, source_path, dll_path) = c_fixture_paths(&native_dir);
        fs::write(
            &header_path,
            "#if defined(_WIN32)\n#define BEACON_EXPORT __declspec(dllexport)\n#else\n#define BEACON_EXPORT\n#endif\nBEACON_EXPORT int beacon_add(int a, int b);\n",
        )
        .expect("header");
        fs::write(
            &source_path,
            "#include \"beacon_math.h\"\nint beacon_add(int a, int b) { return a + b; }\n",
        )
        .expect("source");
        compile_shared_library(&source_path, &dll_path);
        write_c_manifest(root, "beacon_math", &header_path, &dll_path);

        let output = import_library(
            "beacon_math",
            &ImportCOptions {
                mode: ArtifactMode::Both,
                ..ImportCOptions::default()
            },
            &PrepareContext {
                current_dir: Some(root.to_path_buf()),
                manifest_path: Some(root.join("KAIN.toml")),
            },
        )
        .expect("import library");
        let bridge_dylib_path = output
            .dylib_path
            .as_ref()
            .expect("live import should build bridge dylib");
        let copied_bridge_path = package_dir.join(
            output
                .packaged_bridge_manifest
                .bridge_library
                .file_name
                .as_str(),
        );
        let copied_shared_path = package_dir.join(
            output
                .packaged_bridge_manifest
                .shared_library
                .as_ref()
                .expect("shared library descriptor")
                .file_name
                .as_str(),
        );
        fs::copy(bridge_dylib_path, &copied_bridge_path).expect("copy bridge dylib");
        fs::copy(&dll_path, &copied_shared_path).expect("copy shared dll");

        let packaged_manifest = PackagedBridgeManifest {
            schema_version: "kain-c-ffi-runtime-v1".to_string(),
            lane: "c".to_string(),
            imports: vec![output.packaged_bridge_manifest.clone()],
        };
        let packaged_manifest_path = package_dir.join("kain_c_host_bridges.json");
        fs::write(
            &packaged_manifest_path,
            serde_json::to_string_pretty(&packaged_manifest).expect("serialize manifest"),
        )
        .expect("write manifest");

        let loaded = load_packaged_bridges_from_manifest(&packaged_manifest_path)
            .expect("load packaged bridges");
        assert_eq!(loaded, 1);
        let env_key = shared_library_env_var("beacon_math");
        let env_value = std::env::var(&env_key).expect("shared library env var");
        assert!(
            env_value.ends_with(
                copied_shared_path
                    .file_name()
                    .and_then(|value| value.to_str())
                    .expect("shared file name")
            ),
            "shared library env var should point at copied package sidecar"
        );
    }

    #[test]
    fn c_ffi_live_bridge_supports_simple_scalars_and_strings() {
        let temp = TempDir::new().expect("temp dir");
        let root = temp.path();
        let native_dir = root.join("native");
        fs::create_dir_all(&native_dir).expect("native dir");

        let (header_path, source_path, dll_path) = c_fixture_paths(&native_dir);
        fs::write(
            &header_path,
            "#if defined(_WIN32)\n#define BEACON_EXPORT __declspec(dllexport)\n#else\n#define BEACON_EXPORT\n#endif\n\nBEACON_EXPORT int beacon_add(int a, int b);\nBEACON_EXPORT _Bool beacon_is_even(int value);\nBEACON_EXPORT double beacon_scale(double value, double factor);\nBEACON_EXPORT const char* beacon_label(int id);\n",
        )
        .expect("header");
        fs::write(
            &source_path,
            "#include \"beacon_math.h\"\n#include <stdio.h>\nstatic char G_BUFFER[64];\nint beacon_add(int a, int b) { return a + b; }\n_Bool beacon_is_even(int value) { return (value % 2) == 0; }\ndouble beacon_scale(double value, double factor) { return value * factor; }\nconst char* beacon_label(int id) { snprintf(G_BUFFER, sizeof(G_BUFFER), \"beacon-%d\", id); return G_BUFFER; }\n",
        )
        .expect("source");
        compile_shared_library(&source_path, &dll_path);
        fs::write(
            root.join("KAIN.toml"),
            format!(
                "[c_ffi]\n\n[[c_ffi.libraries]]\nname = \"beacon_math\"\nheader = \"{}\"\nshared_lib = \"{}\"\n",
                header_path
                    .strip_prefix(root)
                    .unwrap()
                    .display()
                    .to_string()
                    .replace('\\', "/"),
                dll_path
                    .strip_prefix(root)
                    .unwrap()
                    .display()
                    .to_string()
                    .replace('\\', "/")
            ),
        )
        .expect("manifest");

        let source = "use c::beacon_math\nfn main() -> String:\n    assert(beacon_add(2, 3) == 5, \"expected add\")\n    assert(beacon_is_even(8), \"expected even\")\n    assert(beacon_scale(1.5, 4.0) == 6.0, \"expected scale\")\n    return beacon_label(7)\n";
        let augmented = augment_source_for_runtime(
            source,
            CompileTarget::Interpret,
            &PrepareContext {
                current_dir: Some(root.to_path_buf()),
                manifest_path: Some(root.join("KAIN.toml")),
            },
        )
        .expect("augment");

        let stdlib = kain_core::stdlib::load_stdlib_for_target(CompileTarget::Interpret);
        let full_source = format!("{stdlib}\n{augmented}");
        let tokens = Lexer::new(&full_source).tokenize().expect("tokens");
        let span_mapper = SpanMapper::new(&full_source);
        let mut ast = Parser::new(&tokens, &span_mapper, "<test>")
            .parse()
            .expect("parse");
        kain_core::comptime::eval_program(&mut ast).expect("comptime");
        let typed = types::check(&ast, &span_mapper, "<test>").expect("typecheck");
        let result = interpret(&typed).expect("interpret");
        match result {
            Value::String(value) => assert_eq!(value, "beacon-7"),
            other => panic!("expected String(\"beacon-7\"), got {other:?}"),
        }
    }

    #[test]
    fn c_ffi_can_mutate_shared_images_and_roundtrip_opaque_handles() {
        let temp = TempDir::new().expect("temp dir");
        let root = temp.path();
        let native_dir = root.join("native");
        fs::create_dir_all(&native_dir).expect("native dir");

        let (header_path, source_path, dll_path) = image_fx_fixture_paths(&native_dir);
        fs::write(
            &header_path,
            "#if defined(_WIN32)\n#define IMAGEFX_EXPORT __declspec(dllexport)\n#else\n#define IMAGEFX_EXPORT\n#endif\n#include <stddef.h>\n#include <stdint.h>\ntypedef struct ImageWorkspace ImageWorkspace;\nIMAGEFX_EXPORT uint64_t imagefx_checksum(const uint8_t* pixels, size_t len);\nIMAGEFX_EXPORT void imagefx_invert_rgba(uint8_t* pixels, size_t len);\nIMAGEFX_EXPORT const char* imagefx_signature(int width, int height, uint64_t checksum);\nIMAGEFX_EXPORT ImageWorkspace* imagefx_workspace_create(int width, int height);\nIMAGEFX_EXPORT int imagefx_workspace_area(ImageWorkspace* workspace);\nIMAGEFX_EXPORT void imagefx_workspace_destroy(ImageWorkspace* workspace);\n",
        )
        .expect("header");
        fs::write(
            &source_path,
            "#include \"image_fx.h\"\n#include <stdio.h>\n#include <stdlib.h>\nstruct ImageWorkspace { int width; int height; };\nstatic char G_SIGNATURE[96];\nuint64_t imagefx_checksum(const uint8_t* pixels, size_t len) { uint64_t checksum = 1469598103934665603ull; size_t index = 0; while (index < len) { checksum ^= (uint64_t)pixels[index]; checksum *= 1099511628211ull; index += 1; } return checksum; }\nvoid imagefx_invert_rgba(uint8_t* pixels, size_t len) { size_t index = 0; while (index + 3 < len) { pixels[index] = (uint8_t)(255 - pixels[index]); pixels[index + 1] = (uint8_t)(255 - pixels[index + 1]); pixels[index + 2] = (uint8_t)(255 - pixels[index + 2]); index += 4; } }\nconst char* imagefx_signature(int width, int height, uint64_t checksum) { snprintf(G_SIGNATURE, sizeof(G_SIGNATURE), \"imagefx:%dx%d:%llu\", width, height, (unsigned long long)checksum); return G_SIGNATURE; }\nImageWorkspace* imagefx_workspace_create(int width, int height) { ImageWorkspace* workspace = (ImageWorkspace*)malloc(sizeof(ImageWorkspace)); if (!workspace) { return NULL; } workspace->width = width; workspace->height = height; return workspace; }\nint imagefx_workspace_area(ImageWorkspace* workspace) { if (!workspace) { return 0; } return workspace->width * workspace->height; }\nvoid imagefx_workspace_destroy(ImageWorkspace* workspace) { if (workspace) { free(workspace); } }\n",
        )
        .expect("source");
        compile_shared_library(&source_path, &dll_path);
        fs::write(
            root.join("KAIN.toml"),
            format!(
                "[c_ffi]\n\n[[c_ffi.libraries]]\nname = \"image_fx\"\nheader = \"{}\"\nshared_lib = \"{}\"\n",
                header_path
                    .strip_prefix(root)
                    .unwrap()
                    .display()
                    .to_string()
                    .replace('\\', "/"),
                dll_path
                    .strip_prefix(root)
                    .unwrap()
                    .display()
                    .to_string()
                    .replace('\\', "/")
            ),
        )
        .expect("manifest");

        let source = "use c::image_fx\nfn main() -> String:\n    let bytes = [0, 12, 24, 255, 20, 40, 60, 255]\n    let image = kain_shared_image_from_bytes(bytes, 2, 1, 4, \"HWC\", \"rgba8\", \"image/x-kain-raster\")\n    let info = kain_shared_image_info(image)\n    let before = imagefx_checksum(image, info.byte_length)\n    imagefx_invert_rgba(image, info.byte_length)\n    let after = imagefx_checksum(image, info.byte_length)\n    let workspace = imagefx_workspace_create(info.width, info.height)\n    let area = imagefx_workspace_area(workspace)\n    imagefx_workspace_destroy(workspace)\n    let mutated = kain_shared_image_bytes(image)\n    assert(before != after, \"expected checksum mutation\")\n    assert(mutated[0] == 255, \"expected first channel to invert\")\n    assert(area == info.width * info.height, \"expected opaque handle roundtrip\")\n    return imagefx_signature(info.width, info.height, after)\n";
        let augmented = augment_source_for_runtime(
            source,
            CompileTarget::Interpret,
            &PrepareContext {
                current_dir: Some(root.to_path_buf()),
                manifest_path: Some(root.join("KAIN.toml")),
            },
        )
        .expect("augment");

        kain_interop::register();
        let stdlib = kain_core::stdlib::load_stdlib_for_target(CompileTarget::Interpret);
        let full_source = format!("{stdlib}\n{augmented}");
        let tokens = Lexer::new(&full_source).tokenize().expect("tokens");
        let span_mapper = SpanMapper::new(&full_source);
        let mut ast = Parser::new(&tokens, &span_mapper, "<test>")
            .parse()
            .expect("parse");
        kain_core::comptime::eval_program(&mut ast).expect("comptime");
        let typed = types::check(&ast, &span_mapper, "<test>").expect("typecheck");
        let result = interpret(&typed).expect("interpret");
        match result {
            Value::String(value) => assert!(value.starts_with("imagefx:2x1:")),
            other => panic!("expected imagefx signature, got {other:?}"),
        }
    }

    #[test]
    fn resolve_library_expands_platform_dynamic_library_tokens() {
        let temp = TempDir::new().expect("temp dir");
        let root = temp.path();
        let native_dir = root.join("native");
        fs::create_dir_all(&native_dir).expect("native dir");

        let (header_path, source_path, dylib_path) = c_fixture_paths(&native_dir);
        fs::write(
            &header_path,
            "#if defined(_WIN32)\n#define BEACON_EXPORT __declspec(dllexport)\n#else\n#define BEACON_EXPORT\n#endif\nBEACON_EXPORT int beacon_add(int a, int b);\n",
        )
        .expect("header");
        fs::write(
            &source_path,
            "#include \"beacon_math.h\"\nint beacon_add(int a, int b) { return a + b; }\n",
        )
        .expect("source");
        compile_shared_library(&source_path, &dylib_path);
        fs::write(
            root.join("KAIN.toml"),
            format!(
                "[c_ffi]\n\n[[c_ffi.libraries]]\nname = \"beacon_math\"\nheader = \"{}\"\nshared_lib = \"native/${{kain_dynlib:beacon_math}}\"\n",
                header_path
                    .strip_prefix(root)
                    .unwrap()
                    .display()
                    .to_string()
                    .replace('\\', "/"),
            ),
        )
        .expect("manifest");

        let (resolved, _) = resolve_library(
            "beacon_math",
            &PrepareContext {
                current_dir: Some(root.to_path_buf()),
                manifest_path: Some(root.join("KAIN.toml")),
            },
        )
        .expect("resolve library");

        assert_eq!(resolved.shared_lib_path.as_deref(), Some(dylib_path.as_path()));
    }

    fn c_fixture_paths(native_dir: &Path) -> (PathBuf, PathBuf, PathBuf) {
        let header = native_dir.join("beacon_math.h");
        let source = native_dir.join("beacon_math.c");
        let dll = if cfg!(target_os = "windows") {
            native_dir.join("beacon_math.dll")
        } else if cfg!(target_os = "macos") {
            native_dir.join("libbeacon_math.dylib")
        } else {
            native_dir.join("libbeacon_math.so")
        };
        (header, source, dll)
    }

    fn image_fx_fixture_paths(native_dir: &Path) -> (PathBuf, PathBuf, PathBuf) {
        let header = native_dir.join("image_fx.h");
        let source = native_dir.join("image_fx.c");
        let dll = if cfg!(target_os = "windows") {
            native_dir.join("image_fx.dll")
        } else if cfg!(target_os = "macos") {
            native_dir.join("libimage_fx.dylib")
        } else {
            native_dir.join("libimage_fx.so")
        };
        (header, source, dll)
    }

    fn compile_shared_library(source: &Path, output: &Path) {
        let mut command = Command::new("clang");
        if cfg!(target_os = "windows") {
            command.args(["-shared", "-O2"]);
        } else {
            command.args(["-shared", "-fPIC", "-O2"]);
        }
        let status = command
            .arg(source)
            .arg("-o")
            .arg(output)
            .status()
            .expect("clang should launch for c ffi smoke");
        assert!(status.success(), "clang should build test shared library");
    }

    fn write_c_manifest(root: &Path, import_name: &str, header_path: &Path, dll_path: &Path) {
        fs::write(
            root.join("KAIN.toml"),
            format!(
                "[c_ffi]\n\n[[c_ffi.libraries]]\nname = \"{import_name}\"\nheader = \"{}\"\nshared_lib = \"{}\"\n",
                header_path
                    .strip_prefix(root)
                    .unwrap()
                    .display()
                    .to_string()
                    .replace('\\', "/"),
                dll_path
                    .strip_prefix(root)
                    .unwrap()
                    .display()
                    .to_string()
                    .replace('\\', "/")
            ),
        )
        .expect("manifest");
    }
}
