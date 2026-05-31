mod config;
mod extract;
mod generate;
mod model;
mod platform;

pub use config::{CFfiConfig, CInteropTier, CLibraryConfig};
pub use generate::{bridge_crate_name, BRIDGE_FORMAT_VERSION, BRIDGE_SYMBOL_NAME};
pub use model::{
    ArtifactMode, BindingManifest, BindingReport, BindingReportEntry, CNativeLinkInputs,
    HostBridgeModuleDescriptor, HostBridgeServiceDescriptor, ImportCOptions, ImportCOutput,
    ItemKind, ItemStatus, PackagedBridgeBinaryArtifact, PackagedBridgeImport,
    PackagedBridgeManifest, PackagedBridgeSymbolDescriptor, PrepareContext, ResolvedCLibrary,
};
pub use platform::{
    import_platform_package, ImportPlatformOptions, PlatformBlockedSymbol, PlatformImportOutput,
    PlatformPackageLock, PlatformResolvedFile, PlatformSymbol, PLATFORM_LOCK_SCHEMA_VERSION,
};

use extract::extract_binding_bundle;
use generate::write_generated_artifacts;
use kain_core::error::KainError;
use kain_core::runtime::{register_env_extension, Env};
use kain_core::CompileTarget;
use kain_fs as kfs;
use libloading::{Library, Symbol};
use once_cell::sync::Lazy;
use regex::Regex;
use sha2::{Digest, Sha256};
use std::collections::{BTreeMap, BTreeSet};
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

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct CLibraryImportSpec {
    pub import_name: String,
    pub alias: Option<String>,
    pub origin: CLibraryImportOrigin,
    pub include_target: Option<String>,
    pub include_search: Option<CIncludeSearch>,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CLibraryImportOrigin {
    UseC,
    Include,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CIncludeSearch {
    Local,
    System,
}

impl CLibraryImportSpec {
    fn use_c(import_name: &str) -> Self {
        Self {
            import_name: import_name.to_string(),
            alias: None,
            origin: CLibraryImportOrigin::UseC,
            include_target: None,
            include_search: None,
        }
    }
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
    let imports = detect_c_library_import_specs(source);
    let mut outputs = Vec::new();
    for spec in imports {
        outputs.push(import_library_spec(&spec, options, prepare)?);
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
    let manifest_source = kfs::read_text(manifest_path).map_err(|err| {
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
    import_library_spec(&CLibraryImportSpec::use_c(import_name), options, prepare)
}

fn import_library_spec(
    spec: &CLibraryImportSpec,
    options: &ImportCOptions,
    prepare: &PrepareContext,
) -> Result<ImportCOutput, KainError> {
    register();
    let (resolved, manifest_context) = resolve_library_spec(spec, prepare)?;
    let bundle = extract_binding_bundle(&resolved)?;
    let hash = build_cache_hash(
        &resolved,
        &bundle.source_fingerprints,
        BRIDGE_FORMAT_VERSION,
    );
    let cache_dir = default_cache_root(prepare).join("c_ffi").join(hash);
    kfs::create_dir_all(&cache_dir).map_err(fs_to_kain_error)?;

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
            kfs::create_dir_all(parent).map_err(fs_to_kain_error)?;
        }
        let report_json = serde_json::to_string_pretty(&artifacts.report).map_err(|err| {
            KainError::runtime(format!("Failed to serialize C FFI report override: {err}"))
        })?;
        kfs::atomic_write_text(report_json_path, &report_json).map_err(fs_to_kain_error)?;
    }

    if options.mode.wants_live() {
        if !resolved.tier.is_dynamic() {
            return Err(KainError::runtime(format!(
                "C FFI import '{}' uses tier {:?}, which is native-link only and cannot be loaded by the live dynamic bridge",
                resolved.import_name, resolved.tier
            )));
        }
        let (dylib_path, cache_hit) = ensure_bridge_library(&output)?;
        ensure_bridge_loaded(&dylib_path)?;
        output.dylib_path = Some(dylib_path);
        output.cache_hit = cache_hit;
    }

    Ok(output)
}

pub fn prepare_native_link_inputs(
    output: &ImportCOutput,
    clang_cmd: &str,
    compile_args: &[String],
) -> Result<CNativeLinkInputs, KainError> {
    let resolved = &output.resolved;
    let mut inputs = CNativeLinkInputs {
        link_inputs: Vec::new(),
        link_libs: collect_link_libs(resolved),
    };

    if resolved.native_runtime_linked() {
        return Ok(inputs);
    }

    match resolved.tier {
        CInteropTier::Dynamic => {
            let shared = resolved.shared_lib_path.as_ref().ok_or_else(|| {
                KainError::runtime(format!(
                    "C FFI dynamic import '{}' does not declare a shared library for native linking",
                    resolved.import_name
                ))
            })?;
            inputs
                .link_inputs
                .push(resolve_linkable_shared_library(shared)?);
        }
        CInteropTier::Static => {
            push_existing_paths(&mut inputs.link_inputs, &resolved.object_paths, "object")?;
            push_existing_paths(
                &mut inputs.link_inputs,
                &resolved.static_lib_paths,
                "static library",
            )?;
            push_existing_paths(
                &mut inputs.link_inputs,
                &resolved.bitcode_paths,
                "LLVM bitcode",
            )?;
            if let Some(shared) = &resolved.shared_lib_path {
                inputs
                    .link_inputs
                    .push(resolve_linkable_shared_library(shared)?);
            }
            if inputs.link_inputs.is_empty() && inputs.link_libs.is_empty() {
                return Err(KainError::runtime(format!(
                    "C FFI static import '{}' has no object, static library, bitcode, shared-library link input, or named link library",
                    resolved.import_name
                )));
            }
        }
        CInteropTier::Bitcode | CInteropTier::Inline => {
            push_existing_paths(
                &mut inputs.link_inputs,
                &resolved.bitcode_paths,
                "LLVM bitcode",
            )?;
            let compiled = compile_c_sources_to_bitcode(output, clang_cmd, compile_args)?;
            inputs.link_inputs.extend(compiled);
            if inputs.link_inputs.is_empty() {
                return Err(KainError::runtime(format!(
                    "C FFI {:?} import '{}' requires `sources` or `bitcode` entries",
                    resolved.tier, resolved.import_name
                )));
            }
        }
        CInteropTier::Fused => {
            return Err(KainError::runtime(format!(
                "C FFI fused import '{}' must be runtime-owned in this layer; generic fused call lowering is a compiler/runtime command-surface contract, not a dynamic bridge fallback",
                resolved.import_name
            )));
        }
    }

    Ok(inputs)
}

pub fn augment_source_for_runtime(
    source: &str,
    target: CompileTarget,
    prepare: &PrepareContext,
) -> Result<String, KainError> {
    let imports = detect_c_library_import_specs(source);
    if imports.is_empty() {
        return Ok(source.to_string());
    }

    let mode = artifact_mode_for_target(target).ok_or_else(|| {
        KainError::runtime(
            "C ABI FFI is currently available in Interpret, Test, Rust/native packaging, and LLVM lanes",
        )
    })?;
    let mut outputs = Vec::with_capacity(imports.len());
    for spec in &imports {
        outputs.push((
            spec.clone(),
            import_library_spec(
                spec,
                &ImportCOptions {
                    mode,
                    ..ImportCOptions::default()
                },
                prepare,
            )?,
        ));
    }

    let mut sections = Vec::new();
    for (spec, output) in outputs {
        sections.push(output.canonical_module_source.clone());
        if spec.origin == CLibraryImportOrigin::Include {
            if let Some(alias) = spec.alias.as_deref() {
                if let Some(alias_source) = render_include_alias_source(&output, alias) {
                    sections.push(alias_source);
                }
            }
        }
    }
    sections.push(source.to_string());
    Ok(sections.join("\n"))
}

pub fn detect_c_library_imports(source: &str) -> Vec<String> {
    detect_c_library_import_specs(source)
        .into_iter()
        .map(|spec| spec.import_name)
        .collect()
}

pub fn detect_c_library_import_specs(source: &str) -> Vec<CLibraryImportSpec> {
    static USE_C_REGEX: Lazy<Regex> = Lazy::new(|| {
        Regex::new(r"(?m)^\s*use\s+c(?:::|/)([A-Za-z_][A-Za-z0-9_]*)").expect("regex")
    });
    static INCLUDE_REGEX: Lazy<Regex> = Lazy::new(|| {
        Regex::new(
            r#"(?m)^\s*include\s+(?:"([^"]+)"|<([^>]+)>|([A-Za-z_][A-Za-z0-9_]*(?:[./-][A-Za-z_][A-Za-z0-9_]*)*(?:\.h)?))(?:\s+as\s+([A-Za-z_][A-Za-z0-9_]*))?"#,
        )
        .expect("regex")
    });
    let mut seen = BTreeSet::new();
    let mut imports = Vec::new();
    for captures in USE_C_REGEX.captures_iter(source) {
        if let Some(value) = captures.get(1) {
            let import_name = value.as_str().to_string();
            if seen.insert(import_name.clone()) {
                imports.push(CLibraryImportSpec {
                    import_name,
                    alias: None,
                    origin: CLibraryImportOrigin::UseC,
                    include_target: None,
                    include_search: None,
                });
            }
        }
    }
    for captures in INCLUDE_REGEX.captures_iter(source) {
        let include_target = captures
            .get(1)
            .or_else(|| captures.get(2))
            .or_else(|| captures.get(3))
            .map(|value| value.as_str())
            .unwrap_or_default();
        let import_name = natural_include_import_name(include_target);
        if import_name.is_empty() {
            continue;
        }
        if seen.insert(import_name.clone()) {
            imports.push(CLibraryImportSpec {
                import_name,
                alias: captures.get(4).map(|value| value.as_str().to_string()),
                origin: CLibraryImportOrigin::Include,
                include_target: Some(include_target.to_string()),
                include_search: Some(if captures.get(2).is_some() {
                    CIncludeSearch::System
                } else {
                    CIncludeSearch::Local
                }),
            });
        }
    }
    imports
}

fn natural_include_import_name(target: &str) -> String {
    let normalized = target.replace('\\', "/");
    let file_name = normalized.rsplit('/').next().unwrap_or(&normalized);
    let stem = file_name
        .rsplit_once('.')
        .map(|(left, _)| left)
        .unwrap_or(file_name);
    let mut output = String::with_capacity(stem.len());
    for ch in stem.chars() {
        if ch.is_ascii_alphanumeric() || ch == '_' {
            output.push(ch);
        } else if !output.ends_with('_') {
            output.push('_');
        }
    }
    output.trim_matches('_').to_string()
}

fn canonical_or_self(path: &Path) -> PathBuf {
    std::fs::canonicalize(path).unwrap_or_else(|_| path.to_path_buf())
}

fn render_include_alias_source(output: &ImportCOutput, alias: &str) -> Option<String> {
    if alias.is_empty() {
        return None;
    }

    let mut rendered_aliases = Vec::new();
    let generated_prefix = format!("c_{}_", output.resolved.import_name);
    let mut pending_attributes = Vec::new();
    for line in output.canonical_module_source.lines() {
        let trimmed = line.trim_start();
        if trimmed.starts_with('@') && !trimmed.starts_with("@extern fn ") {
            pending_attributes.push(trimmed.to_string());
            continue;
        }
        let Some(signature) = trimmed.strip_prefix("@extern fn ") else {
            pending_attributes.clear();
            continue;
        };
        let Some(paren_index) = signature.find('(') else {
            pending_attributes.clear();
            continue;
        };
        let raw_name = &signature[..paren_index];
        if raw_name.starts_with(&generated_prefix) {
            pending_attributes.clear();
            continue;
        }
        let Some(alias_name) =
            include_alias_function_name(&output.resolved.import_name, alias, raw_name)
        else {
            pending_attributes.clear();
            continue;
        };
        let signature_tail = &signature[paren_index..];
        let mut alias_decl = String::new();
        if !pending_attributes.is_empty() {
            alias_decl.push_str(&pending_attributes.join("\n"));
            alias_decl.push('\n');
        }
        alias_decl.push_str(&format!(
            "@link_name(\"{}\")\n@extern fn {}{}",
            raw_name, alias_name, signature_tail
        ));
        rendered_aliases.push(alias_decl);
        pending_attributes.clear();
    }

    if rendered_aliases.is_empty() {
        return None;
    }

    let mut output_source = String::new();
    output_source.push_str(&format!(
        "# Generated include alias surface for {} as {}\n",
        output.resolved.import_name, alias
    ));
    output_source.push_str(&rendered_aliases.join("\n"));
    output_source.push('\n');
    Some(output_source)
}

fn include_alias_function_name(import_name: &str, alias: &str, raw_name: &str) -> Option<String> {
    let local = c_symbol_without_namespace_prefix(raw_name, import_name)
        .or_else(|| c_symbol_without_namespace_prefix(raw_name, alias))
        .unwrap_or(raw_name);
    let mut sanitized = String::with_capacity(alias.len() + local.len() + 1);
    sanitized.push_str(alias);
    sanitized.push('_');
    for ch in local.chars() {
        if ch.is_ascii_alphanumeric() || ch == '_' {
            sanitized.push(ch);
        } else if !sanitized.ends_with('_') {
            sanitized.push('_');
        }
    }
    let sanitized = sanitized.trim_end_matches('_').to_string();
    if sanitized == format!("{alias}_") {
        None
    } else {
        Some(sanitized)
    }
}

fn c_symbol_without_namespace_prefix<'a>(raw_name: &'a str, prefix: &str) -> Option<&'a str> {
    raw_name
        .strip_prefix(prefix)
        .map(|value| value.trim_start_matches('_'))
        .filter(|value| !value.is_empty())
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
        CompileTarget::C => Some(ArtifactMode::Generate),
        CompileTarget::Llvm => Some(ArtifactMode::Generate),
        _ => None,
    }
}

fn ensure_bridge_loaded(dylib_path: &Path) -> Result<(), KainError> {
    register();
    let canonical_path = kfs::canonicalize_path(dylib_path)
        .map(PathBuf::from)
        .unwrap_or_else(|_| dylib_path.to_path_buf());
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

fn collect_link_libs(resolved: &ResolvedCLibrary) -> Vec<String> {
    let mut seen = BTreeSet::new();
    let mut link_libs = Vec::new();
    for value in resolved
        .global_config
        .link_libs
        .iter()
        .chain(resolved.config.link_libs.iter())
    {
        if seen.insert(value.clone()) {
            link_libs.push(value.clone());
        }
    }
    link_libs
}

fn push_existing_paths(
    output: &mut Vec<PathBuf>,
    paths: &[PathBuf],
    label: &str,
) -> Result<(), KainError> {
    for path in paths {
        if !path.exists() {
            return Err(KainError::runtime(format!(
                "C FFI {label} link input '{}' does not exist",
                path.display()
            )));
        }
        output.push(path.clone());
    }
    Ok(())
}

fn resolve_linkable_shared_library(shared_lib_path: &Path) -> Result<PathBuf, KainError> {
    if !shared_lib_path.exists() {
        return Err(KainError::runtime(format!(
            "C FFI shared library {} does not exist",
            shared_lib_path.display()
        )));
    }
    if cfg!(windows)
        && shared_lib_path
            .extension()
            .map(|ext| ext.to_string_lossy().eq_ignore_ascii_case("dll"))
            .unwrap_or(false)
    {
        let import_library_path = shared_lib_path.with_extension("lib");
        if import_library_path.exists() {
            return Ok(import_library_path);
        }
    }
    Ok(shared_lib_path.to_path_buf())
}

fn compile_c_sources_to_bitcode(
    output: &ImportCOutput,
    clang_cmd: &str,
    compile_args: &[String],
) -> Result<Vec<PathBuf>, KainError> {
    let resolved = &output.resolved;
    let mut compiled = Vec::new();
    if resolved.source_paths.is_empty() {
        return Ok(compiled);
    }

    let bitcode_dir = output.cache_dir.join("bitcode");
    kfs::create_dir_all(&bitcode_dir).map_err(fs_to_kain_error)?;
    for (index, source_path) in resolved.source_paths.iter().enumerate() {
        if !source_path.exists() {
            return Err(KainError::runtime(format!(
                "C FFI source '{}' does not exist",
                source_path.display()
            )));
        }
        let stem = source_path
            .file_stem()
            .and_then(|value| value.to_str())
            .unwrap_or("source")
            .replace(|ch: char| !ch.is_ascii_alphanumeric(), "_");
        let bitcode_path =
            bitcode_dir.join(format!("{}_{}_{}.bc", resolved.import_name, index, stem));
        let mut command = Command::new(clang_cmd);
        for arg in compile_args {
            command.arg(arg);
        }
        command.arg("-emit-llvm").arg("-c");
        if matches!(resolved.tier, CInteropTier::Inline) {
            command.arg("-flto=full");
        }
        for include_path in resolved
            .global_config
            .include_paths
            .iter()
            .chain(resolved.config.include_paths.iter())
        {
            command.arg("-I").arg(include_path);
        }
        for define in resolved
            .global_config
            .defines
            .iter()
            .chain(resolved.config.defines.iter())
        {
            command.arg(format!("-D{define}"));
        }
        for option in resolved
            .global_config
            .cpp_options
            .iter()
            .chain(resolved.config.cpp_options.iter())
        {
            command.arg(option);
        }
        let status = command
            .arg(source_path)
            .arg("-o")
            .arg(&bitcode_path)
            .status()
            .map_err(|err| {
                KainError::runtime(format!(
                    "Failed to launch clang for C FFI {:?} source '{}': {err}",
                    resolved.tier,
                    source_path.display()
                ))
            })?;
        if !status.success() {
            return Err(KainError::runtime(format!(
                "clang failed while compiling C FFI {:?} source '{}' to LLVM bitcode",
                resolved.tier,
                source_path.display()
            )));
        }
        compiled.push(bitcode_path);
    }
    Ok(compiled)
}

fn resolve_library(
    import_name: &str,
    prepare: &PrepareContext,
) -> Result<(ResolvedCLibrary, model::ManifestContext), KainError> {
    resolve_library_spec(&CLibraryImportSpec::use_c(import_name), prepare)
}

fn resolve_library_spec(
    spec: &CLibraryImportSpec,
    prepare: &PrepareContext,
) -> Result<(ResolvedCLibrary, model::ManifestContext), KainError> {
    let import_name = spec.import_name.as_str();
    let start_dir = prepare
        .manifest_path
        .as_ref()
        .and_then(|value| value.parent().map(Path::to_path_buf))
        .or_else(|| prepare.current_dir.clone())
        .unwrap_or_else(|| std::env::current_dir().unwrap_or_else(|_| PathBuf::from(".")));
    if let Some(manifest_root) = find_kain_manifest_root(&start_dir) {
        if let Some(resolved) = resolve_library_from_manifest_root(import_name, &manifest_root)? {
            return Ok(resolved);
        }
    }

    if let Some((blade, _library)) = blade::resolve_c_ffi_library_blade(&start_dir, import_name)
        .map_err(|err| {
            KainError::runtime(format!(
                "C FFI blade discovery failed while resolving '{import_name}': {err}"
            ))
        })?
    {
        if let Some(resolved) = resolve_library_from_manifest_root(import_name, &blade.root)? {
            return Ok(resolved);
        }
    }

    if let Some(resolved) = resolve_natural_local_include(spec, &start_dir)? {
        return Ok(resolved);
    }

    if let Some(resolved) = resolve_system_include(spec, &start_dir)? {
        return Ok(resolved);
    }

    if let Some(resolved) = resolve_runtime_owned_header(import_name, &start_dir) {
        return Ok(resolved);
    }

    Err(KainError::runtime(format!(
        "Could not resolve C FFI import '{import_name}' from nearest KAIN.toml, discovered blades, local headers, or runtime/native/include"
    )))
}

fn resolve_natural_local_include(
    spec: &CLibraryImportSpec,
    start_dir: &Path,
) -> Result<Option<(ResolvedCLibrary, model::ManifestContext)>, KainError> {
    if spec.include_search == Some(CIncludeSearch::System) {
        return Ok(None);
    }
    let Some(header_path) = find_natural_local_header(spec, start_dir) else {
        return Ok(None);
    };
    let include_dir = header_path
        .parent()
        .map(Path::to_path_buf)
        .unwrap_or_else(|| start_dir.to_path_buf());
    let source_paths = natural_local_c_sources(&header_path);
    if source_paths.is_empty() {
        return Err(KainError::runtime(format!(
            "Natural C include '{}' found header '{}' but no sibling .c source file to link",
            spec.import_name,
            header_path.display()
        )));
    }

    let library = config::CLibraryConfig {
        name: spec.import_name.clone(),
        header: header_path.clone(),
        shared_lib: None,
        symbols: BTreeMap::new(),
        include_paths: vec![include_dir.clone()],
        defines: Vec::new(),
        link_libs: Vec::new(),
        sources: source_paths.clone(),
        objects: Vec::new(),
        static_libs: Vec::new(),
        bitcode: Vec::new(),
        cpp_options: Vec::new(),
        cpp_command: None,
        tier: Some(config::CInteropTier::Inline),
        runtime_owned: false,
    };
    let global_config = config::CFfiConfig {
        include_paths: vec![include_dir],
        defines: Vec::new(),
        link_libs: Vec::new(),
        cpp_options: Vec::new(),
        cpp_command: None,
        tier: config::CInteropTier::Inline,
        libraries: vec![library.clone()],
    };
    let manifest_root =
        find_kain_manifest_root(start_dir).unwrap_or_else(|| start_dir.to_path_buf());

    Ok(Some((
        ResolvedCLibrary {
            import_name: spec.import_name.clone(),
            manifest_root,
            header_path,
            shared_lib_path: None,
            source_paths,
            object_paths: Vec::new(),
            static_lib_paths: Vec::new(),
            bitcode_paths: Vec::new(),
            config: library,
            global_config: global_config.clone(),
            tier: config::CInteropTier::Inline,
            runtime_owned: false,
        },
        model::ManifestContext {
            root_dir: None,
            config: Some(global_config),
        },
    )))
}

fn find_natural_local_header(spec: &CLibraryImportSpec, start_dir: &Path) -> Option<PathBuf> {
    if let Some(include_target) = spec.include_target.as_deref() {
        for candidate in explicit_local_header_candidates(include_target, start_dir) {
            if candidate.is_file() {
                return Some(canonical_or_self(&candidate));
            }
        }
    }

    let wanted = format!("{}.h", spec.import_name);
    let direct_roots = [
        start_dir.to_path_buf(),
        start_dir.join("native"),
        start_dir.join("include"),
        start_dir.join("src"),
    ];
    for root in direct_roots {
        let candidate = root.join(&wanted);
        if candidate.is_file() {
            return Some(canonical_or_self(&candidate));
        }
    }

    let search_root = find_kain_manifest_root(start_dir).unwrap_or_else(|| start_dir.to_path_buf());
    let mut matches = Vec::new();
    collect_natural_header_candidates(&search_root, &wanted, &mut matches, 4096);
    matches.sort_by(|left, right| {
        left.components()
            .count()
            .cmp(&right.components().count())
            .then_with(|| left.display().to_string().cmp(&right.display().to_string()))
    });
    matches.into_iter().next()
}

fn explicit_local_header_candidates(include_target: &str, start_dir: &Path) -> Vec<PathBuf> {
    let normalized = include_target.replace('\\', "/");
    let target_path = Path::new(&normalized);
    let mut candidates = Vec::new();
    if target_path.is_absolute() {
        candidates.push(target_path.to_path_buf());
        if target_path.extension().is_none() {
            candidates.push(target_path.with_extension("h"));
        }
        return candidates;
    }

    for root in [
        start_dir.to_path_buf(),
        start_dir.join("src"),
        start_dir.join("native"),
        start_dir.join("include"),
    ] {
        let candidate = root.join(target_path);
        candidates.push(candidate.clone());
        if candidate.extension().is_none() {
            candidates.push(candidate.with_extension("h"));
        }
    }
    candidates
}

fn resolve_system_include(
    spec: &CLibraryImportSpec,
    start_dir: &Path,
) -> Result<Option<(ResolvedCLibrary, model::ManifestContext)>, KainError> {
    if spec.include_search != Some(CIncludeSearch::System) {
        return Ok(None);
    }
    let Some(include_target) = spec.include_target.as_deref() else {
        return Ok(None);
    };
    let normalized_target = include_target.replace('\\', "/");
    let family = classify_system_include_family(&normalized_target).ok_or_else(|| {
        KainError::runtime(format!(
            "System C include '<{}>' is not supported yet; the current system-header lane understands C runtime headers, Windows SDK headers, and Vulkan SDK headers",
            include_target
        ))
    })?;
    let header_name = system_header_file_name(&normalized_target);
    let manifest_root =
        find_kain_manifest_root(start_dir).unwrap_or_else(|| start_dir.to_path_buf());

    let (mut include_paths, link_libs) = match family {
        SystemIncludeFamily::CRuntime => (
            collect_c_runtime_include_roots(),
            current_platform_c_runtime_link_libs(header_name),
        ),
        SystemIncludeFamily::WindowsSdk => (
            collect_windows_sdk_include_roots(),
            current_platform_windows_sdk_link_libs(header_name),
        ),
        SystemIncludeFamily::Vulkan => (
            collect_vulkan_include_roots(),
            current_platform_vulkan_link_libs(),
        ),
    };
    let header_path = find_system_header_under_roots(&normalized_target, &include_paths)
        .ok_or_else(|| {
            let searched = if include_paths.is_empty() {
                "<none>".to_string()
            } else {
                include_paths
                    .iter()
                    .map(|path| path.display().to_string())
                    .collect::<Vec<_>>()
                    .join(", ")
            };
            KainError::runtime(format!(
                "System C include '<{}>' could not be resolved from include roots: {}",
                include_target, searched
            ))
        })?;
    let header_parent = header_path
        .parent()
        .map(Path::to_path_buf)
        .unwrap_or_else(|| manifest_root.clone());
    push_existing_unique_path(&mut include_paths, header_parent.clone());
    if include_paths.is_empty() {
        include_paths.push(header_parent);
    }

    let library = config::CLibraryConfig {
        name: spec.import_name.clone(),
        header: header_path.clone(),
        shared_lib: None,
        symbols: BTreeMap::new(),
        include_paths: include_paths.clone(),
        defines: Vec::new(),
        link_libs: link_libs.clone(),
        sources: Vec::new(),
        objects: Vec::new(),
        static_libs: Vec::new(),
        bitcode: Vec::new(),
        cpp_options: Vec::new(),
        cpp_command: None,
        tier: Some(config::CInteropTier::Static),
        runtime_owned: false,
    };
    let global_config = config::CFfiConfig {
        include_paths,
        defines: Vec::new(),
        link_libs: Vec::new(),
        cpp_options: Vec::new(),
        cpp_command: None,
        tier: config::CInteropTier::Static,
        libraries: vec![library.clone()],
    };

    Ok(Some((
        ResolvedCLibrary {
            import_name: spec.import_name.clone(),
            manifest_root,
            header_path,
            shared_lib_path: None,
            source_paths: Vec::new(),
            object_paths: Vec::new(),
            static_lib_paths: Vec::new(),
            bitcode_paths: Vec::new(),
            config: library,
            global_config: global_config.clone(),
            tier: config::CInteropTier::Static,
            runtime_owned: false,
        },
        model::ManifestContext {
            root_dir: None,
            config: Some(global_config),
        },
    )))
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum SystemIncludeFamily {
    CRuntime,
    WindowsSdk,
    Vulkan,
}

fn classify_system_include_family(target: &str) -> Option<SystemIncludeFamily> {
    let normalized = target.replace('\\', "/").to_ascii_lowercase();
    let header_name = system_header_file_name(&normalized);
    if normalized == "vulkan/vulkan.h" || header_name == "vulkan.h" {
        return Some(SystemIncludeFamily::Vulkan);
    }
    if is_c_runtime_header(header_name) {
        return Some(SystemIncludeFamily::CRuntime);
    }
    if cfg!(windows) && header_name.ends_with(".h") {
        return Some(SystemIncludeFamily::WindowsSdk);
    }
    None
}

fn system_header_file_name(target: &str) -> &str {
    target.rsplit('/').next().unwrap_or(target)
}

fn is_c_runtime_header(header_name: &str) -> bool {
    matches!(
        header_name,
        "assert.h"
            | "ctype.h"
            | "errno.h"
            | "float.h"
            | "inttypes.h"
            | "limits.h"
            | "locale.h"
            | "math.h"
            | "setjmp.h"
            | "signal.h"
            | "stdarg.h"
            | "stdbool.h"
            | "stddef.h"
            | "stdint.h"
            | "stdio.h"
            | "stdlib.h"
            | "string.h"
            | "time.h"
            | "uchar.h"
            | "wchar.h"
            | "wctype.h"
    )
}

fn collect_c_runtime_include_roots() -> Vec<PathBuf> {
    let mut roots = collect_generic_system_include_roots();
    if cfg!(windows) {
        for path in collect_windows_sdk_include_roots() {
            push_existing_unique_path(&mut roots, path);
        }
        if let Some(vc_tools_dir) = existing_env_dir("VCToolsInstallDir") {
            push_existing_unique_path(&mut roots, vc_tools_dir.join("include"));
        }
    } else if cfg!(target_os = "macos") {
        push_existing_unique_path(&mut roots, PathBuf::from("/usr/include"));
        push_existing_unique_path(
            &mut roots,
            PathBuf::from("/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include"),
        );
    } else {
        push_existing_unique_path(&mut roots, PathBuf::from("/usr/include"));
        push_existing_unique_path(&mut roots, PathBuf::from("/usr/local/include"));
    }
    roots
}

fn collect_vulkan_include_roots() -> Vec<PathBuf> {
    let mut roots = collect_generic_system_include_roots();
    for sdk_root in collect_vulkan_sdk_roots() {
        push_existing_unique_path(&mut roots, sdk_root.join("Include"));
        push_existing_unique_path(&mut roots, sdk_root.join("include"));
    }
    roots
}

fn collect_windows_sdk_include_roots() -> Vec<PathBuf> {
    let mut roots = collect_generic_system_include_roots();
    if !cfg!(windows) {
        return roots;
    }

    for sdk_root in collect_windows_sdk_roots() {
        append_windows_sdk_include_dirs(&mut roots, &sdk_root);
    }
    roots
}

fn collect_generic_system_include_roots() -> Vec<PathBuf> {
    let mut roots = Vec::new();
    for env_name in [
        "KAIN_C_FFI_SYSTEM_INCLUDE_ROOTS",
        "INCLUDE",
        "C_INCLUDE_PATH",
        "CPATH",
        "CPLUS_INCLUDE_PATH",
    ] {
        append_env_split_paths(&mut roots, env_name);
    }
    roots
}

fn collect_vulkan_sdk_roots() -> Vec<PathBuf> {
    let mut roots = Vec::new();
    for env_name in [
        "KAIN_PLATFORM_VULKAN_SDK_ROOT",
        "KAIN_PLATFORM_VULKAN_SDK",
        "VULKAN_SDK",
    ] {
        if let Some(path) = existing_env_dir(env_name) {
            push_existing_unique_path(&mut roots, path);
        }
    }
    if cfg!(windows) {
        let sdk_parent = PathBuf::from(r"C:\VulkanSDK");
        append_latest_versioned_children(&mut roots, &sdk_parent);
    }
    roots
}

fn collect_windows_sdk_roots() -> Vec<PathBuf> {
    let mut roots = Vec::new();
    for env_name in [
        "KAIN_PLATFORM_WINDOWS_SDK_ROOT",
        "KAIN_PLATFORM_WINDOWS_SDK",
        "WindowsSdkDir",
    ] {
        if let Some(path) = existing_env_dir(env_name) {
            push_existing_unique_path(&mut roots, path);
        }
    }
    if cfg!(windows) {
        push_existing_unique_path(
            &mut roots,
            PathBuf::from(r"C:\Program Files (x86)\Windows Kits\10"),
        );
        push_existing_unique_path(
            &mut roots,
            PathBuf::from(r"C:\Program Files\Windows Kits\10"),
        );
    }
    roots
}

fn append_windows_sdk_include_dirs(output: &mut Vec<PathBuf>, sdk_root: &Path) {
    let include_root = sdk_root.join("Include");
    if !include_root.is_dir() {
        return;
    }

    let explicit_version = std::env::var("WindowsSDKVersion")
        .ok()
        .map(|value| {
            value
                .trim()
                .trim_end_matches('\\')
                .trim_end_matches('/')
                .to_string()
        })
        .filter(|value| !value.is_empty());
    let version_root = explicit_version
        .as_deref()
        .map(|value| include_root.join(value))
        .filter(|path| path.is_dir())
        .or_else(|| discover_latest_child_dir(&include_root));

    if let Some(version_root) = version_root {
        for lane in ["ucrt", "shared", "um", "winrt", "cppwinrt"] {
            push_existing_unique_path(output, version_root.join(lane));
        }
    }
}

fn append_latest_versioned_children(output: &mut Vec<PathBuf>, root: &Path) {
    let mut candidates = match std::fs::read_dir(root) {
        Ok(entries) => entries
            .filter_map(|entry| entry.ok())
            .map(|entry| entry.path())
            .filter(|path| path.is_dir())
            .collect::<Vec<_>>(),
        Err(_) => return,
    };
    candidates.sort_by(|left, right| {
        right
            .file_name()
            .unwrap_or_default()
            .cmp(left.file_name().unwrap_or_default())
    });
    for candidate in candidates {
        push_existing_unique_path(output, candidate);
    }
}

fn discover_latest_child_dir(root: &Path) -> Option<PathBuf> {
    let mut candidates = std::fs::read_dir(root)
        .ok()?
        .filter_map(|entry| entry.ok())
        .map(|entry| entry.path())
        .filter(|path| path.is_dir())
        .collect::<Vec<_>>();
    candidates.sort_by(|left, right| {
        right
            .file_name()
            .unwrap_or_default()
            .cmp(left.file_name().unwrap_or_default())
    });
    candidates.into_iter().next()
}

fn find_system_header_under_roots(target: &str, roots: &[PathBuf]) -> Option<PathBuf> {
    let normalized = target.replace('\\', "/");
    let wanted_name = system_header_file_name(&normalized).to_ascii_lowercase();
    for root in roots {
        let candidate = root.join(Path::new(&normalized));
        if candidate.is_file() {
            return Some(canonical_or_self(&candidate));
        }
        let fallback = root.join(system_header_file_name(&normalized));
        if fallback.is_file() {
            return Some(canonical_or_self(&fallback));
        }
    }

    for root in roots {
        if let Some(found) = find_first_header_by_name(root, &wanted_name, 4096) {
            return Some(found);
        }
    }
    None
}

fn find_first_header_by_name(dir: &Path, wanted_name: &str, mut budget: usize) -> Option<PathBuf> {
    if budget == 0 || !dir.is_dir() {
        return None;
    }
    let entries = std::fs::read_dir(dir).ok()?;
    for entry in entries.flatten() {
        if budget == 0 {
            break;
        }
        budget -= 1;
        let path = entry.path();
        if path.is_dir() {
            if let Some(found) = find_first_header_by_name(&path, wanted_name, budget) {
                return Some(found);
            }
        } else if path
            .file_name()
            .and_then(|value| value.to_str())
            .is_some_and(|name| name.eq_ignore_ascii_case(wanted_name))
        {
            return Some(canonical_or_self(&path));
        }
    }
    None
}

fn current_platform_c_runtime_link_libs(header_name: &str) -> Vec<String> {
    let header_name = header_name.to_ascii_lowercase();
    if cfg!(windows) {
        let mut libs = vec!["ucrt".to_string()];
        if matches!(header_name.as_str(), "stdio.h" | "stdlib.h") {
            libs.push("legacy_stdio_definitions".to_string());
        }
        libs
    } else if cfg!(target_os = "macos") {
        vec!["System".to_string()]
    } else if header_name == "math.h" {
        vec!["m".to_string(), "c".to_string()]
    } else {
        vec!["c".to_string()]
    }
}

fn current_platform_vulkan_link_libs() -> Vec<String> {
    if cfg!(windows) {
        vec!["vulkan-1".to_string()]
    } else if cfg!(target_os = "macos") {
        vec!["MoltenVK".to_string()]
    } else {
        vec!["vulkan".to_string()]
    }
}

fn current_platform_windows_sdk_link_libs(header_name: &str) -> Vec<String> {
    let lower = header_name.to_ascii_lowercase();
    match lower.as_str() {
        "winsock2.h" | "ws2tcpip.h" => vec!["ws2_32".to_string()],
        "commctrl.h" => vec![
            "comctl32".to_string(),
            "user32".to_string(),
            "gdi32".to_string(),
        ],
        "shellapi.h" | "shlobj.h" | "shobjidl.h" => {
            vec![
                "shell32".to_string(),
                "ole32".to_string(),
                "uuid".to_string(),
            ]
        }
        "imm.h" => vec!["imm32".to_string(), "user32".to_string()],
        "d3d12.h" => vec![
            "d3d12".to_string(),
            "dxgi".to_string(),
            "dxguid".to_string(),
        ],
        header if header.starts_with("dxgi") => vec!["dxgi".to_string(), "dxguid".to_string()],
        _ => vec![
            "user32".to_string(),
            "gdi32".to_string(),
            "advapi32".to_string(),
            "ole32".to_string(),
            "shell32".to_string(),
            "comdlg32".to_string(),
            "comctl32".to_string(),
            "uuid".to_string(),
            "imm32".to_string(),
            "ws2_32".to_string(),
        ],
    }
}

fn append_env_split_paths(output: &mut Vec<PathBuf>, env_name: &str) {
    let Some(raw) = std::env::var_os(env_name) else {
        return;
    };
    for path in std::env::split_paths(&raw) {
        if !path.as_os_str().is_empty() {
            push_existing_unique_path(output, path);
        }
    }
}

fn existing_env_dir(env_name: &str) -> Option<PathBuf> {
    let raw = std::env::var_os(env_name)?;
    let path = PathBuf::from(raw);
    path.is_dir().then_some(path)
}

fn push_existing_unique_path(output: &mut Vec<PathBuf>, candidate: PathBuf) {
    if candidate.is_dir() && !output.iter().any(|path| path == &candidate) {
        output.push(candidate);
    }
}

fn collect_natural_header_candidates(
    dir: &Path,
    wanted: &str,
    matches: &mut Vec<PathBuf>,
    mut budget: usize,
) -> usize {
    if budget == 0 || should_skip_natural_include_dir(dir) {
        return budget;
    }
    let Ok(entries) = std::fs::read_dir(dir) else {
        return budget;
    };
    for entry in entries.flatten() {
        if budget == 0 {
            break;
        }
        budget -= 1;
        let path = entry.path();
        if path.is_dir() {
            budget = collect_natural_header_candidates(&path, wanted, matches, budget);
        } else if path
            .file_name()
            .and_then(|value| value.to_str())
            .is_some_and(|name| name.eq_ignore_ascii_case(wanted))
        {
            matches.push(canonical_or_self(&path));
        }
    }
    budget
}

fn should_skip_natural_include_dir(path: &Path) -> bool {
    path.file_name()
        .and_then(|value| value.to_str())
        .is_some_and(|name| {
            matches!(
                name,
                ".git" | ".kain" | "target" | "node_modules" | ".venv" | "__pycache__"
            ) || name.starts_with("bazel-")
        })
}

fn natural_local_c_sources(header_path: &Path) -> Vec<PathBuf> {
    let Some(parent) = header_path.parent() else {
        return Vec::new();
    };
    let Some(stem) = header_path.file_stem().and_then(|value| value.to_str()) else {
        return Vec::new();
    };
    let candidate = parent.join(format!("{stem}.c"));
    if candidate.is_file() {
        vec![canonical_or_self(&candidate)]
    } else {
        Vec::new()
    }
}

fn resolve_library_from_manifest_root(
    import_name: &str,
    manifest_root: &Path,
) -> Result<Option<(ResolvedCLibrary, model::ManifestContext)>, KainError> {
    let Some(config) = load_c_ffi_config(manifest_root)? else {
        return Ok(None);
    };
    let Some(library) = config
        .libraries
        .iter()
        .find(|value| value.name == import_name)
        .cloned()
    else {
        return Ok(None);
    };

    let header_path = resolve_relative_path(manifest_root, &library.header);
    if !header_path.exists() {
        return Err(KainError::runtime(format!(
            "C FFI header '{}' does not exist",
            header_path.display()
        )));
    }
    let shared_lib_path = library
        .shared_lib
        .as_ref()
        .map(|value| resolve_relative_path(manifest_root, value));
    let tier = library.tier.unwrap_or(config.tier);
    let runtime_owned = library.runtime_owned;
    let mut library = library;
    library.include_paths = resolve_relative_paths(manifest_root, &library.include_paths);
    library.sources = resolve_relative_paths(manifest_root, &library.sources);
    library.objects = resolve_relative_paths(manifest_root, &library.objects);
    library.static_libs = resolve_relative_paths(manifest_root, &library.static_libs);
    library.bitcode = resolve_relative_paths(manifest_root, &library.bitcode);
    let mut config = config;
    config.include_paths = resolve_relative_paths(manifest_root, &config.include_paths);
    validate_tier_contract(import_name, tier, runtime_owned, &library)?;

    Ok(Some((
        ResolvedCLibrary {
            import_name: import_name.to_string(),
            manifest_root: manifest_root.to_path_buf(),
            header_path,
            shared_lib_path,
            source_paths: library.sources.clone(),
            object_paths: library.objects.clone(),
            static_lib_paths: library.static_libs.clone(),
            bitcode_paths: library.bitcode.clone(),
            config: library,
            global_config: config.clone(),
            tier,
            runtime_owned,
        },
        model::ManifestContext {
            root_dir: Some(manifest_root.to_path_buf()),
            config: Some(config),
        },
    )))
}

fn load_c_ffi_config(root: &Path) -> Result<Option<config::CFfiConfig>, KainError> {
    for name in ["KAIN.toml", "kain.toml"] {
        let manifest_path = root.join(name);
        if !manifest_path.exists() {
            continue;
        }
        let source = kfs::read_text(&manifest_path).map_err(fs_to_kain_error)?;
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

fn resolve_runtime_owned_header(
    import_name: &str,
    start_dir: &Path,
) -> Option<(ResolvedCLibrary, model::ManifestContext)> {
    let repo_root = find_repo_root_with_runtime_include(start_dir)?;
    let include_dir = repo_root.join("runtime").join("native").join("include");
    let header_name = runtime_header_candidates(import_name)
        .into_iter()
        .find(|candidate| include_dir.join(candidate).exists())?;
    let header_path = include_dir.join(&header_name);
    let library = config::CLibraryConfig {
        name: import_name.to_string(),
        header: header_path.clone(),
        shared_lib: None,
        symbols: BTreeMap::new(),
        include_paths: vec![include_dir.clone()],
        defines: Vec::new(),
        link_libs: Vec::new(),
        sources: Vec::new(),
        objects: Vec::new(),
        static_libs: Vec::new(),
        bitcode: Vec::new(),
        cpp_options: Vec::new(),
        cpp_command: None,
        tier: Some(config::CInteropTier::Static),
        runtime_owned: true,
    };
    let global_config = config::CFfiConfig {
        include_paths: vec![include_dir],
        defines: Vec::new(),
        link_libs: Vec::new(),
        cpp_options: Vec::new(),
        cpp_command: None,
        tier: config::CInteropTier::Static,
        libraries: vec![library.clone()],
    };

    Some((
        ResolvedCLibrary {
            import_name: import_name.to_string(),
            manifest_root: repo_root,
            header_path,
            shared_lib_path: None,
            source_paths: Vec::new(),
            object_paths: Vec::new(),
            static_lib_paths: Vec::new(),
            bitcode_paths: Vec::new(),
            config: library,
            global_config: global_config.clone(),
            tier: config::CInteropTier::Static,
            runtime_owned: true,
        },
        model::ManifestContext {
            root_dir: None,
            config: Some(global_config),
        },
    ))
}

fn runtime_header_candidates(import_name: &str) -> Vec<String> {
    let normalized = import_name.replace('-', "_");
    let mut candidates = vec![format!("{normalized}.h")];
    if !normalized.ends_with("_system") {
        candidates.push(format!("{normalized}_system.h"));
    }
    if normalized == "net" {
        candidates.push("net_system.h".to_string());
    } else if normalized == "process" {
        candidates.push("process_system.h".to_string());
    } else if normalized == "graphics" {
        candidates.push("graphics_system.h".to_string());
    } else if normalized == "input" {
        candidates.push("input_system.h".to_string());
    } else if normalized == "ui" {
        candidates.push("ui_system.h".to_string());
    }
    candidates.sort();
    candidates.dedup();
    candidates
}

fn find_repo_root_with_runtime_include(start_dir: &Path) -> Option<PathBuf> {
    let mut current = Some(start_dir);
    while let Some(dir) = current {
        if dir.join("runtime").join("native").join("include").is_dir() {
            return Some(dir.to_path_buf());
        }
        current = dir.parent();
    }
    None
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

fn resolve_relative_paths(root: &Path, paths: &[PathBuf]) -> Vec<PathBuf> {
    paths
        .iter()
        .map(|path| resolve_relative_path(root, path))
        .collect()
}

fn validate_tier_contract(
    import_name: &str,
    tier: CInteropTier,
    runtime_owned: bool,
    library: &config::CLibraryConfig,
) -> Result<(), KainError> {
    if runtime_owned {
        return Ok(());
    }
    if tier.is_fused() {
        return Err(KainError::runtime(format!(
            "C FFI fused import '{import_name}' must set runtime_owned = true; generic fused lowering is not a dynamic bridge"
        )));
    }
    if tier.wants_llvm_bitcode() && library.sources.is_empty() && library.bitcode.is_empty() {
        return Err(KainError::runtime(format!(
            "C FFI {:?} import '{import_name}' requires `sources` or `bitcode` entries",
            tier
        )));
    }
    Ok(())
}

fn expand_platform_dynamic_library_tokens(path: &Path) -> PathBuf {
    let source = path.to_string_lossy();
    let mut expanded = source.into_owned();
    const DYNLIB_TOKEN_PREFIX: &str = "${kain_dynlib:";
    const ENV_TOKEN_PREFIX: &str = "${env:";

    while let Some(start) = expanded.find(DYNLIB_TOKEN_PREFIX) {
        let token_end = expanded[start..]
            .find('}')
            .map(|offset| start + offset)
            .unwrap_or(expanded.len());
        if token_end >= expanded.len() {
            break;
        }
        let library_stem = &expanded[start + DYNLIB_TOKEN_PREFIX.len()..token_end];
        let replacement = current_platform_dynamic_library_name(library_stem);
        expanded.replace_range(start..=token_end, &replacement);
    }

    while let Some(start) = expanded.find(ENV_TOKEN_PREFIX) {
        let token_end = expanded[start..]
            .find('}')
            .map(|offset| start + offset)
            .unwrap_or(expanded.len());
        if token_end >= expanded.len() {
            break;
        }
        let variable = &expanded[start + ENV_TOKEN_PREFIX.len()..token_end];
        let replacement = std::env::var(variable).unwrap_or_default();
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

fn fs_to_kain_error(error: kain_fs::FsError) -> KainError {
    KainError::runtime(format!("Filesystem error: {error}"))
}

#[cfg(test)]
mod tests {
    use super::*;
    use kain_core::diagnostics::SpanMapper;
    use kain_core::lexer::Lexer;
    use kain_core::parser::Parser;
    use kain_core::runtime::{interpret, Value};
    use kain_core::types;
    use tempfile::TempDir;

    #[test]
    fn detects_c_imports() {
        let source = "use c::beacon_math\nuse c::image_fx\ninclude native/tiny_math.h as tm\ninclude <vulkan/vulkan.h> as vk\n";
        assert_eq!(
            detect_c_library_imports(source),
            vec![
                "beacon_math".to_string(),
                "image_fx".to_string(),
                "tiny_math".to_string(),
                "vulkan".to_string()
            ]
        );
        let specs = detect_c_library_import_specs(source);
        assert_eq!(specs[2].alias.as_deref(), Some("tm"));
        assert_eq!(specs[2].origin, CLibraryImportOrigin::Include);
        assert_eq!(
            specs[2].include_target.as_deref(),
            Some("native/tiny_math.h")
        );
        assert_eq!(specs[2].include_search, Some(CIncludeSearch::Local));
        assert_eq!(specs[3].alias.as_deref(), Some("vk"));
        assert_eq!(specs[3].include_target.as_deref(), Some("vulkan/vulkan.h"));
        assert_eq!(specs[3].include_search, Some(CIncludeSearch::System));
    }

    #[test]
    fn include_alias_names_strip_import_or_alias_prefixes() {
        assert_eq!(
            include_alias_function_name("sqlite3", "sql", "sqlite3_open").as_deref(),
            Some("sql_open")
        );
        assert_eq!(
            include_alias_function_name("nuklear", "nk", "nk_strlen").as_deref(),
            Some("nk_strlen")
        );
        assert_eq!(
            include_alias_function_name("tiny_math", "tm", "native_add").as_deref(),
            Some("tm_native_add")
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
        let message = error.to_string();
        assert!(message.contains("Interpret, Test"));
        assert!(message.contains("LLVM"));
    }

    #[test]
    fn rust_target_prepares_generated_c_bridge_without_live_loading() {
        let temp = TempDir::new().expect("temp dir");
        let root = temp.path();
        let native_dir = root.join("native");
        kfs::create_dir_all(&native_dir).expect("native dir");

        let (header_path, source_path, dll_path) = c_fixture_paths(&native_dir);
        kfs::write_text(
            &header_path,
            "#if defined(_WIN32)\n#define BEACON_EXPORT __declspec(dllexport)\n#else\n#define BEACON_EXPORT\n#endif\nBEACON_EXPORT int beacon_add(int a, int b);\n",
        )
        .expect("header");
        kfs::write_text(
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
    fn llvm_target_prepares_generated_c_bridge_as_extern_declarations() {
        let temp = TempDir::new().expect("temp dir");
        let root = temp.path();
        let native_dir = root.join("native");
        kfs::create_dir_all(&native_dir).expect("native dir");

        let (header_path, source_path, dll_path) = c_fixture_paths(&native_dir);
        kfs::write_text(
            &header_path,
            "#if defined(_WIN32)\n#define BEACON_EXPORT __declspec(dllexport)\n#else\n#define BEACON_EXPORT\n#endif\nBEACON_EXPORT int beacon_add(int a, int b);\nBEACON_EXPORT const char* beacon_label(int id);\nBEACON_EXPORT int beacon_ping(void);\n",
        )
        .expect("header");
        kfs::write_text(
            &source_path,
            "#include \"beacon_math.h\"\n#include <stdio.h>\nstatic char G_BUFFER[64];\nint beacon_add(int a, int b) { return a + b; }\nconst char* beacon_label(int id) { snprintf(G_BUFFER, sizeof(G_BUFFER), \"beacon-%d\", id); return G_BUFFER; }\nint beacon_ping(void) { return 1; }\n",
        )
        .expect("source");
        compile_shared_library(&source_path, &dll_path);
        write_c_manifest(root, "beacon_math", &header_path, &dll_path);

        let augmented = augment_source_for_runtime(
            "use c::beacon_math\nfn main() -> Int:\n    let ping = beacon_ping(())\n    return beacon_add(ping, 3)\n",
            CompileTarget::Llvm,
            &PrepareContext {
                current_dir: Some(root.to_path_buf()),
                manifest_path: Some(root.join("KAIN.toml")),
            },
        )
        .expect("llvm target should accept generated c bridge bindings");

        assert!(augmented.contains("@extern fn"));
        assert!(augmented.contains("beacon_add"));
        assert!(augmented.contains("beacon_ping"));
    }

    #[test]
    fn natural_include_resolves_local_header_and_sibling_c_source() {
        let temp = TempDir::new().expect("temp dir");
        let root = temp.path();
        let src_dir = root.join("src");
        let native_dir = src_dir.join("native");
        kfs::create_dir_all(&native_dir).expect("native dir");
        let header_path = native_dir.join("tiny_math.h");
        let source_path = native_dir.join("tiny_math.c");
        kfs::write_text(&header_path, "int tiny_math_add(int a, int b);\n").expect("header");
        kfs::write_text(
            &source_path,
            "#include \"tiny_math.h\"\nint tiny_math_add(int a, int b) { return a + b; }\n",
        )
        .expect("source");

        let augmented = augment_source_for_runtime(
            "include native/tiny_math.h as tm\nfn main() -> Int:\n    return tiny_math_add(2, 3)\n",
            CompileTarget::Llvm,
            &PrepareContext {
                current_dir: Some(src_dir),
                manifest_path: None,
            },
        )
        .expect("natural include should prepare local header");

        assert!(augmented.contains("mod c:"));
        assert!(augmented.contains("tiny_math_add"));
        assert!(augmented.contains("@link_name(\"tiny_math_add\")"));
        assert!(augmented.contains("@extern fn tm_add"));

        let outputs = import_libraries_for_source(
            "include native/tiny_math.h as tm\n",
            &ImportCOptions {
                mode: ArtifactMode::Generate,
                ..ImportCOptions::default()
            },
            &PrepareContext {
                current_dir: Some(root.join("src")),
                manifest_path: None,
            },
        )
        .expect("natural include output");
        assert_eq!(outputs.len(), 1);
        assert_eq!(
            outputs[0].resolved.header_path,
            canonical_or_self(&header_path)
        );
        assert_eq!(
            outputs[0].resolved.source_paths,
            vec![canonical_or_self(&source_path)]
        );
        assert_eq!(outputs[0].resolved.tier, CInteropTier::Inline);
    }

    #[test]
    fn system_include_resolves_vulkan_sdk_header_from_env_root() {
        let temp = TempDir::new().expect("temp dir");
        let root = temp.path();
        let sdk_root = root.join("vulkan_sdk");
        let include_root = sdk_root.join("Include");
        let header_path = include_root.join("vulkan").join("vulkan.h");
        kfs::create_dir_all(header_path.parent().expect("vulkan include parent"))
            .expect("sdk include dir");
        kfs::write_text(
            &header_path,
            "typedef void* VkInstance;\nvoid* vkGetInstanceProcAddr(VkInstance instance, const char* pName);\n",
        )
        .expect("header");

        unsafe {
            std::env::set_var("KAIN_PLATFORM_VULKAN_SDK", &sdk_root);
        }

        let outputs = import_libraries_for_source(
            "include <vulkan/vulkan.h> as vk\n",
            &ImportCOptions {
                mode: ArtifactMode::Generate,
                ..ImportCOptions::default()
            },
            &PrepareContext {
                current_dir: Some(root.to_path_buf()),
                manifest_path: None,
            },
        )
        .expect("system include output");

        assert_eq!(outputs.len(), 1);
        assert_eq!(
            outputs[0].resolved.header_path,
            canonical_or_self(&header_path)
        );
        assert_eq!(outputs[0].resolved.tier, CInteropTier::Static);
        assert!(outputs[0]
            .resolved
            .config
            .link_libs
            .iter()
            .any(|value| value.eq_ignore_ascii_case("vulkan-1")
                || value.eq_ignore_ascii_case("vulkan")));

        let link_inputs = prepare_native_link_inputs(&outputs[0], "clang", &["-O2".to_string()])
            .expect("link policy");
        assert!(link_inputs.link_inputs.is_empty());
        assert!(!link_inputs.link_libs.is_empty());
    }

    #[cfg(windows)]
    #[test]
    fn system_include_resolves_windows_sdk_header_from_env_root() {
        let temp = TempDir::new().expect("temp dir");
        let root = temp.path();
        let sdk_root = root.join("windows_sdk");
        let include_version_root = sdk_root.join("Include").join("10.0.26100.0");
        let header_path = include_version_root.join("um").join("windows.h");
        kfs::create_dir_all(header_path.parent().expect("windows include parent"))
            .expect("windows sdk include dir");
        kfs::write_text(&header_path, "int MessageBoxA(void* hwnd, const char* text, const char* caption, unsigned int ty);\n")
            .expect("header");

        unsafe {
            std::env::set_var("KAIN_PLATFORM_WINDOWS_SDK", &sdk_root);
            std::env::set_var("WindowsSDKVersion", "10.0.26100.0");
        }

        let outputs = import_libraries_for_source(
            "include <windows.h> as win\n",
            &ImportCOptions {
                mode: ArtifactMode::Generate,
                ..ImportCOptions::default()
            },
            &PrepareContext {
                current_dir: Some(root.to_path_buf()),
                manifest_path: None,
            },
        )
        .expect("windows system include output");

        assert_eq!(outputs.len(), 1);
        assert_eq!(
            outputs[0].resolved.header_path,
            canonical_or_self(&header_path)
        );
        assert_eq!(outputs[0].resolved.tier, CInteropTier::Static);
        assert!(outputs[0]
            .resolved
            .config
            .link_libs
            .iter()
            .any(|value| value.eq_ignore_ascii_case("user32")));
        let link_inputs = prepare_native_link_inputs(&outputs[0], "clang", &["-O2".to_string()])
            .expect("link policy");
        assert!(link_inputs.link_inputs.is_empty());
        assert!(!link_inputs.link_libs.is_empty());
    }

    #[test]
    fn natural_include_alias_surface_typechecks_without_use_c_imports() {
        let temp = TempDir::new().expect("temp dir");
        let root = temp.path();
        let src_dir = root.join("src");
        let native_dir = src_dir.join("native");
        kfs::create_dir_all(&native_dir).expect("native dir");
        kfs::write_text(
            native_dir.join("tiny_math.h"),
            "int tiny_math_add(int a, int b);\nint tiny_math_ping(void);\n",
        )
        .expect("header");
        kfs::write_text(
            native_dir.join("tiny_math.c"),
            "#include \"tiny_math.h\"\nint tiny_math_add(int a, int b) { return a + b; }\nint tiny_math_ping(void) { return 1; }\n",
        )
        .expect("source");

        let source = "\
include native/tiny_math.h as tm

fn main() -> Int:
    let ping = tm_ping()
    return tm_add(ping, 3)
";
        let augmented = augment_source_for_runtime(
            source,
            CompileTarget::Llvm,
            &PrepareContext {
                current_dir: Some(src_dir),
                manifest_path: None,
            },
        )
        .expect("natural include should prepare alias surface");

        assert!(augmented.contains("@extern fn tm_add"));
        assert!(augmented.contains("@extern fn tm_ping"));
        assert!(!augmented.contains("use c::tiny_math"));

        let tokens = Lexer::new(&augmented).tokenize().expect("tokens");
        let mapper = SpanMapper::new(&augmented);
        let ast = Parser::new(&tokens, &mapper, "<include-alias-surface>")
            .parse()
            .expect("parse");
        types::check(&ast, &mapper, "<include-alias-surface>")
            .expect("include alias surface should typecheck");
    }

    #[test]
    fn natural_include_marks_c_string_returns_on_canonical_and_alias_surfaces() {
        let temp = TempDir::new().expect("temp dir");
        let root = temp.path();
        let src_dir = root.join("src");
        let native_dir = src_dir.join("native");
        kfs::create_dir_all(&native_dir).expect("native dir");
        kfs::write_text(
            native_dir.join("tiny_math.h"),
            "const char* tiny_math_label(int id);\n",
        )
        .expect("header");
        kfs::write_text(
            native_dir.join("tiny_math.c"),
            "#include \"tiny_math.h\"\nconst char* tiny_math_label(int id) { (void)id; return \"tiny-math\"; }\n",
        )
        .expect("source");

        let augmented = augment_source_for_runtime(
            "include native/tiny_math.h as tm\nfn main() -> Int:\n    return len(tm_label(7))\n",
            CompileTarget::Llvm,
            &PrepareContext {
                current_dir: Some(src_dir),
                manifest_path: None,
            },
        )
        .expect("natural include should prepare c string return surface");

        assert!(augmented.contains("@c_string_return\n        @extern fn tiny_math_label"));
        assert!(augmented
            .contains("@c_string_return\n@link_name(\"tiny_math_label\")\n@extern fn tm_label"));
    }

    #[test]
    fn inline_tier_compiles_c_sources_to_llvm_bitcode_link_inputs() {
        let temp = TempDir::new().expect("temp dir");
        let root = temp.path();
        let native_dir = root.join("native");
        kfs::create_dir_all(&native_dir).expect("native dir");

        let (header_path, source_path, _dll_path) = c_fixture_paths(&native_dir);
        kfs::write_text(
            &header_path,
            "#if defined(_WIN32)\n#define BEACON_EXPORT __declspec(dllexport)\n#else\n#define BEACON_EXPORT\n#endif\nBEACON_EXPORT int beacon_add(int a, int b);\n",
        )
        .expect("header");
        kfs::write_text(
            &source_path,
            "#include \"beacon_math.h\"\nint beacon_add(int a, int b) { return a + b; }\n",
        )
        .expect("source");
        kfs::write_text(
            root.join("KAIN.toml"),
            &format!(
                "[c_ffi]\n\n[[c_ffi.libraries]]\nname = \"beacon_math\"\ntier = \"inline\"\nheader = \"{}\"\nsources = [\"{}\"]\ninclude_paths = [\"native\"]\n",
                header_path
                    .strip_prefix(root)
                    .unwrap()
                    .display()
                    .to_string()
                    .replace('\\', "/"),
                source_path
                    .strip_prefix(root)
                    .unwrap()
                    .display()
                    .to_string()
                    .replace('\\', "/"),
            ),
        )
        .expect("manifest");

        let output = import_library(
            "beacon_math",
            &ImportCOptions {
                mode: ArtifactMode::Generate,
                ..ImportCOptions::default()
            },
            &PrepareContext {
                current_dir: Some(root.to_path_buf()),
                manifest_path: Some(root.join("KAIN.toml")),
            },
        )
        .expect("inline source-backed import");

        assert_eq!(output.resolved.tier, config::CInteropTier::Inline);
        assert!(output.resolved.source_backed_bitcode());
        assert_eq!(output.resolved.source_paths, vec![source_path.clone()]);
        let report_json = kfs::read_text(&output.report_json_path).expect("report");
        assert!(report_json.contains("\"interop_tier\": \"inline\""));
        assert!(report_json.contains("beacon_math.c"));

        let link_inputs =
            prepare_native_link_inputs(&output, "clang", &["-O2".to_string()]).expect("bitcode");
        assert_eq!(link_inputs.link_inputs.len(), 1);
        assert_eq!(
            link_inputs.link_inputs[0]
                .extension()
                .and_then(|value| value.to_str()),
            Some("bc")
        );
        assert!(link_inputs.link_inputs[0].exists());
    }

    #[test]
    fn fused_tier_rejects_generic_dynamic_bridge_fallback() {
        let temp = TempDir::new().expect("temp dir");
        let root = temp.path();
        let native_dir = root.join("native");
        kfs::create_dir_all(&native_dir).expect("native dir");

        let (header_path, source_path, _dll_path) = c_fixture_paths(&native_dir);
        kfs::write_text(&header_path, "int beacon_add(int a, int b);\n").expect("header");
        kfs::write_text(
            &source_path,
            "#include \"beacon_math.h\"\nint beacon_add(int a, int b) { return a + b; }\n",
        )
        .expect("source");
        kfs::write_text(
            root.join("KAIN.toml"),
            &format!(
                "[c_ffi]\n\n[[c_ffi.libraries]]\nname = \"beacon_math\"\ntier = \"fused\"\nheader = \"{}\"\nsources = [\"{}\"]\n",
                header_path
                    .strip_prefix(root)
                    .unwrap()
                    .display()
                    .to_string()
                    .replace('\\', "/"),
                source_path
                    .strip_prefix(root)
                    .unwrap()
                    .display()
                    .to_string()
                    .replace('\\', "/"),
            ),
        )
        .expect("manifest");

        let error = import_library(
            "beacon_math",
            &ImportCOptions {
                mode: ArtifactMode::Generate,
                ..ImportCOptions::default()
            },
            &PrepareContext {
                current_dir: Some(root.to_path_buf()),
                manifest_path: Some(root.join("KAIN.toml")),
            },
        )
        .expect_err("generic fused import should be gated");
        let message = error.to_string();
        assert!(message.contains("fused"));
        assert!(message.contains("runtime_owned"));
    }

    #[test]
    fn runtime_owned_headers_resolve_without_manifest_ceremony() {
        let repo_root = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
            .parent()
            .and_then(|path| path.parent())
            .expect("repo root")
            .to_path_buf();

        let output = import_library(
            "version",
            &ImportCOptions {
                mode: ArtifactMode::Generate,
                ..ImportCOptions::default()
            },
            &PrepareContext {
                current_dir: Some(repo_root.join("runtime").join("blades")),
                manifest_path: None,
            },
        )
        .expect("runtime header import should resolve from runtime/native/include");

        assert!(output.resolved.runtime_owned);
        assert!(output.resolved.native_runtime_linked());
        assert_eq!(output.resolved.tier, config::CInteropTier::Static);
        assert!(output
            .resolved
            .header_path
            .ends_with(Path::new("runtime/native/include/version.h")));
        assert!(output.canonical_module_source.contains("mod c:"));
        assert!(output.canonical_module_source.contains("c_out:"));
        assert!(!output.canonical_module_source.contains(" out:"));
        assert!(output.report_json_path.exists());
    }

    #[test]
    fn runtime_owned_header_augmented_source_parses() {
        let repo_root = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
            .parent()
            .and_then(|path| path.parent())
            .expect("repo root")
            .to_path_buf();
        let source =
            "use c::version\nfn main() -> Int:\n    return version_check_abi_compatibility(256)\n";
        let augmented = augment_source_for_runtime(
            source,
            CompileTarget::Llvm,
            &PrepareContext {
                current_dir: Some(repo_root.join("runtime").join("blades")),
                manifest_path: None,
            },
        )
        .expect("augment runtime header");
        let tokens = Lexer::new(&augmented).tokenize().expect("tokens");
        let span_mapper = SpanMapper::new(&augmented);
        Parser::new(&tokens, &span_mapper, "<runtime-c-ffi-test>")
            .parse()
            .unwrap_or_else(|err| {
                panic!("generated runtime C import should parse: {err}\n{augmented}")
            });
    }

    #[test]
    fn c_ffi_v2_classifies_raw_pointer_callback_and_pointer_return_shapes() {
        let temp = TempDir::new().expect("temp dir");
        let root = temp.path();
        let native_dir = root.join("native");
        kfs::create_dir_all(&native_dir).expect("native dir");

        let header_path = native_dir.join("raw_api.h");
        kfs::write_text(
            &header_path,
            "#include <stdint.h>\n#include <stddef.h>\ntypedef struct RawDevice RawDevice;\ntypedef void (*rawapi_callback_t)(void* user, int code);\nint rawapi_register(RawDevice** out_device, int* status, const void** chain, rawapi_callback_t callback, void* user);\nuint8_t* rawapi_bytes(void);\n",
        )
        .expect("header");
        kfs::write_text(
            root.join("KAIN.toml"),
            &format!(
                "[c_ffi]\n\n[[c_ffi.libraries]]\nname = \"raw_api\"\nheader = \"{}\"\n",
                header_path
                    .strip_prefix(root)
                    .unwrap()
                    .display()
                    .to_string()
                    .replace('\\', "/"),
            ),
        )
        .expect("manifest");

        let output = import_library(
            "raw_api",
            &ImportCOptions {
                mode: ArtifactMode::Generate,
                ..ImportCOptions::default()
            },
            &PrepareContext {
                current_dir: Some(root.to_path_buf()),
                manifest_path: Some(root.join("KAIN.toml")),
            },
        )
        .expect("raw api import should classify v2 shapes");

        let report_json = kfs::read_text(&output.report_json_path).expect("report");
        assert!(report_json.contains("\"raw-pointer\""));
        assert!(report_json.contains("\"callback-pointer\""));
        assert!(
            !report_json.contains("\"status\": \"unsupported\""),
            "raw pointer/callback/byte pointer return shapes should no longer be unsupported:\n{report_json}"
        );
        assert!(output.canonical_module_source.contains("rawapi_register"));
        assert!(output.canonical_module_source.contains("rawapi_bytes"));
    }

    #[test]
    fn packaged_bridge_manifest_loads_prebuilt_bridge_from_copied_sidecars() {
        let temp = TempDir::new().expect("temp dir");
        let root = temp.path();
        let native_dir = root.join("native");
        let package_dir = root.join("package");
        kfs::create_dir_all(&native_dir).expect("native dir");
        kfs::create_dir_all(&package_dir).expect("package dir");

        let (header_path, source_path, dll_path) = c_fixture_paths(&native_dir);
        kfs::write_text(
            &header_path,
            "#if defined(_WIN32)\n#define BEACON_EXPORT __declspec(dllexport)\n#else\n#define BEACON_EXPORT\n#endif\nBEACON_EXPORT int beacon_add(int a, int b);\n",
        )
        .expect("header");
        kfs::write_text(
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
        kfs::copy_file(bridge_dylib_path, &copied_bridge_path).expect("copy bridge dylib");
        kfs::copy_file(&dll_path, &copied_shared_path).expect("copy shared dll");

        let packaged_manifest = PackagedBridgeManifest {
            schema_version: "kain-c-ffi-runtime-v1".to_string(),
            lane: "c".to_string(),
            imports: vec![output.packaged_bridge_manifest.clone()],
        };
        let packaged_manifest_path = package_dir.join("kain_c_host_bridges.json");
        kfs::write_text(
            &packaged_manifest_path,
            &serde_json::to_string_pretty(&packaged_manifest).expect("serialize manifest"),
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
        kfs::create_dir_all(&native_dir).expect("native dir");

        let (header_path, source_path, dll_path) = c_fixture_paths(&native_dir);
        kfs::write_text(
            &header_path,
            "#if defined(_WIN32)\n#define BEACON_EXPORT __declspec(dllexport)\n#else\n#define BEACON_EXPORT\n#endif\n\nBEACON_EXPORT int beacon_add(int a, int b);\nBEACON_EXPORT _Bool beacon_is_even(int value);\nBEACON_EXPORT double beacon_scale(double value, double factor);\nBEACON_EXPORT const char* beacon_label(int id);\n",
        )
        .expect("header");
        kfs::write_text(
            &source_path,
            "#include \"beacon_math.h\"\n#include <stdio.h>\nstatic char G_BUFFER[64];\nint beacon_add(int a, int b) { return a + b; }\n_Bool beacon_is_even(int value) { return (value % 2) == 0; }\ndouble beacon_scale(double value, double factor) { return value * factor; }\nconst char* beacon_label(int id) { snprintf(G_BUFFER, sizeof(G_BUFFER), \"beacon-%d\", id); return G_BUFFER; }\n",
        )
        .expect("source");
        compile_shared_library(&source_path, &dll_path);
        kfs::write_text(
            root.join("KAIN.toml"),
            &format!(
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
        kfs::create_dir_all(&native_dir).expect("native dir");

        let (header_path, source_path, dll_path) = image_fx_fixture_paths(&native_dir);
        kfs::write_text(
            &header_path,
            "#if defined(_WIN32)\n#define IMAGEFX_EXPORT __declspec(dllexport)\n#else\n#define IMAGEFX_EXPORT\n#endif\n#include <stddef.h>\n#include <stdint.h>\ntypedef struct ImageWorkspace ImageWorkspace;\nIMAGEFX_EXPORT uint64_t imagefx_checksum(const uint8_t* pixels, size_t len);\nIMAGEFX_EXPORT void imagefx_invert_rgba(uint8_t* pixels, size_t len);\nIMAGEFX_EXPORT const char* imagefx_signature(int width, int height, uint64_t checksum);\nIMAGEFX_EXPORT ImageWorkspace* imagefx_workspace_create(int width, int height);\nIMAGEFX_EXPORT int imagefx_workspace_area(ImageWorkspace* workspace);\nIMAGEFX_EXPORT void imagefx_workspace_destroy(ImageWorkspace* workspace);\n",
        )
        .expect("header");
        kfs::write_text(
            &source_path,
            "#include \"image_fx.h\"\n#include <stdio.h>\n#include <stdlib.h>\nstruct ImageWorkspace { int width; int height; };\nstatic char G_SIGNATURE[96];\nuint64_t imagefx_checksum(const uint8_t* pixels, size_t len) { uint64_t checksum = 1469598103934665603ull; size_t index = 0; while (index < len) { checksum ^= (uint64_t)pixels[index]; checksum *= 1099511628211ull; index += 1; } return checksum; }\nvoid imagefx_invert_rgba(uint8_t* pixels, size_t len) { size_t index = 0; while (index + 3 < len) { pixels[index] = (uint8_t)(255 - pixels[index]); pixels[index + 1] = (uint8_t)(255 - pixels[index + 1]); pixels[index + 2] = (uint8_t)(255 - pixels[index + 2]); index += 4; } }\nconst char* imagefx_signature(int width, int height, uint64_t checksum) { snprintf(G_SIGNATURE, sizeof(G_SIGNATURE), \"imagefx:%dx%d:%llu\", width, height, (unsigned long long)checksum); return G_SIGNATURE; }\nImageWorkspace* imagefx_workspace_create(int width, int height) { ImageWorkspace* workspace = (ImageWorkspace*)malloc(sizeof(ImageWorkspace)); if (!workspace) { return NULL; } workspace->width = width; workspace->height = height; return workspace; }\nint imagefx_workspace_area(ImageWorkspace* workspace) { if (!workspace) { return 0; } return workspace->width * workspace->height; }\nvoid imagefx_workspace_destroy(ImageWorkspace* workspace) { if (workspace) { free(workspace); } }\n",
        )
        .expect("source");
        compile_shared_library(&source_path, &dll_path);
        kfs::write_text(
            root.join("KAIN.toml"),
            &format!(
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
        kfs::create_dir_all(&native_dir).expect("native dir");

        let (header_path, source_path, dylib_path) = c_fixture_paths(&native_dir);
        kfs::write_text(
            &header_path,
            "#if defined(_WIN32)\n#define BEACON_EXPORT __declspec(dllexport)\n#else\n#define BEACON_EXPORT\n#endif\nBEACON_EXPORT int beacon_add(int a, int b);\n",
        )
        .expect("header");
        kfs::write_text(
            &source_path,
            "#include \"beacon_math.h\"\nint beacon_add(int a, int b) { return a + b; }\n",
        )
        .expect("source");
        compile_shared_library(&source_path, &dylib_path);
        kfs::write_text(
            root.join("KAIN.toml"),
            &format!(
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

        assert_eq!(
            resolved.shared_lib_path.as_deref(),
            Some(dylib_path.as_path())
        );
    }

    #[test]
    fn resolve_library_expands_env_path_tokens_for_inline_includes() {
        let temp = TempDir::new().expect("temp dir");
        let root = temp.path();
        let sdk_dir = root.join("sdk");
        let include_dir = sdk_dir.join("Include");
        let native_dir = root.join("native");
        kfs::create_dir_all(&include_dir).expect("include dir");
        kfs::create_dir_all(&native_dir).expect("native dir");

        let (header_path, source_path, _dylib_path) = c_fixture_paths(&native_dir);
        kfs::write_text(
            &header_path,
            "#if defined(_WIN32)\n#define BEACON_EXPORT __declspec(dllexport)\n#else\n#define BEACON_EXPORT\n#endif\nBEACON_EXPORT int beacon_add(int a, int b);\n",
        )
        .expect("header");
        kfs::write_text(
            &source_path,
            "#include \"beacon_math.h\"\nint beacon_add(int a, int b) { return a + b; }\n",
        )
        .expect("source");
        unsafe {
            std::env::set_var("KAIN_C_FFI_TEST_SDK", &sdk_dir);
        }
        kfs::write_text(
            root.join("KAIN.toml"),
            &format!(
                "[c_ffi]\n\n[[c_ffi.libraries]]\nname = \"beacon_math\"\ntier = \"inline\"\nheader = \"{}\"\nsources = [\"{}\"]\ninclude_paths = [\"${{env:KAIN_C_FFI_TEST_SDK}}/Include\"]\n",
                header_path
                    .strip_prefix(root)
                    .unwrap()
                    .display()
                    .to_string()
                    .replace('\\', "/"),
                source_path
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

        assert_eq!(resolved.config.include_paths, vec![include_dir]);
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
        kain_core::install_layout::apply_windows_msvc_link_env(&mut command);
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
        kfs::write_text(
            root.join("KAIN.toml"),
            &format!(
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
