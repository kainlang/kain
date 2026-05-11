use crate::config::{RustFfiConfig, RustFfiPathCrate, RustFfiRegistryCrate};
use crate::model::{
    DependencySpec, ImportCrateOptions, PrepareContext, ResolutionKind, ResolvedCrate,
};
use kain_core::error::KainError;
use kain_fs as kfs;
use serde::Deserialize;
use sha2::{Digest, Sha256};
use std::collections::{BTreeSet, HashSet};
use std::ffi::OsStr;
use std::path::{Path, PathBuf};
use std::process::Command;

const KAIN_MANIFEST_NAMES: &[&str] = &["KAIN.toml", "kain.toml"];
const CARGO_MANIFEST_NAME: &str = "Cargo.toml";

#[derive(Debug, Clone, Deserialize)]
struct CargoMetadata {
    workspace_root: String,
    packages: Vec<CargoPackage>,
    #[serde(default)]
    workspace_members: Vec<String>,
}

#[derive(Debug, Clone, Deserialize)]
struct CargoPackage {
    id: String,
    name: String,
    version: String,
    manifest_path: String,
    targets: Vec<CargoTarget>,
}

#[derive(Debug, Clone, Deserialize)]
struct CargoTarget {
    name: String,
    kind: Vec<String>,
    src_path: String,
}

#[derive(Debug, Clone)]
pub struct ManifestContext {
    pub root_dir: Option<PathBuf>,
    pub config: Option<RustFfiConfig>,
    pub manifest_path: Option<PathBuf>,
}

#[derive(Debug, Clone)]
pub struct CacheInputs {
    pub target_triple: String,
    pub rustc_version: String,
    pub source_file_hashes: Vec<(PathBuf, String)>,
}

pub fn resolve_crate(
    crate_name: &str,
    options: &ImportCrateOptions,
    prepare: &PrepareContext,
) -> Result<(ResolvedCrate, ManifestContext), KainError> {
    let start_dir = prepare
        .current_dir
        .clone()
        .or_else(|| std::env::current_dir().ok())
        .unwrap_or_else(|| PathBuf::from("."));
    let manifest_context = load_manifest_context(&start_dir, options, prepare)?;

    if let Some(crate_path) = options.crate_path.as_ref() {
        let resolved = resolve_explicit_crate_path(crate_name, crate_path, options)?;
        return Ok((resolved, manifest_context));
    }

    let mut metadata_error = None;
    if let Some(manifest_path) = manifest_context.manifest_path.as_ref() {
        match load_metadata(manifest_path, options) {
            Ok(metadata) => {
                if let Some(resolved) = resolve_from_workspace_or_dependencies(
                    crate_name,
                    manifest_path,
                    &metadata,
                    options,
                )? {
                    return Ok((resolved, manifest_context));
                }
            }
            Err(err) => {
                metadata_error = Some(err);
            }
        }
    }

    if let Some(config) = manifest_context.config.as_ref() {
        if let Some(path_entry) = config
            .path_crates
            .iter()
            .find(|entry| entry.name == crate_name)
        {
            let resolved =
                resolve_config_path_crate(crate_name, path_entry, &manifest_context, options)?;
            return Ok((resolved, manifest_context));
        }

        if let Some(registry_entry) = config
            .registry_crates
            .iter()
            .find(|entry| entry.name == crate_name)
        {
            let resolved = resolve_registry_crate(crate_name, registry_entry, options)?;
            return Ok((resolved, manifest_context));
        }
    }

    if let Some(resolved) = resolve_blade_crate(crate_name, &start_dir, options)? {
        return Ok((resolved, manifest_context));
    }

    let mut searched = Vec::new();
    if let Some(manifest_path) = manifest_context.manifest_path.as_ref() {
        searched.push(format!(
            "workspace/dependencies via {}",
            manifest_path.display()
        ));
    }
    if let Some(root) = manifest_context.root_dir.as_ref() {
        searched.push(format!("KAIN manifest at {}", root.display()));
    }
    if options.crate_path.is_some() {
        searched.push("--crate-path".to_string());
    }
    if searched.is_empty() {
        searched.push(start_dir.display().to_string());
    }

    if let Some(err) = metadata_error {
        return Err(KainError::runtime(format!(
            "Rust crate FFI could not resolve crate '{crate_name}'. Workspace/dependency resolution failed before fallback search: {err}"
        )));
    }

    Err(KainError::runtime(format!(
        "Rust crate FFI could not resolve crate '{crate_name}'. Searched: {}",
        searched.join(", ")
    )))
}

fn resolve_blade_crate(
    crate_name: &str,
    start_dir: &Path,
    options: &ImportCrateOptions,
) -> Result<Option<ResolvedCrate>, KainError> {
    let Some(blade) =
        kain_blades::resolve_rust_crate_blade(start_dir, crate_name).map_err(|err| {
            KainError::runtime(format!(
                "Rust crate FFI blade discovery failed while resolving '{crate_name}': {err}"
            ))
        })?
    else {
        return Ok(None);
    };
    let Some(cargo_manifest) = blade.cargo_manifest else {
        return Ok(None);
    };
    resolve_explicit_crate_path(crate_name, &cargo_manifest, options).map(|mut resolved| {
        resolved.resolution_kind = ResolutionKind::PathConfig;
        Some(resolved)
    })
}

pub fn load_manifest_context(
    start_dir: &Path,
    options: &ImportCrateOptions,
    prepare: &PrepareContext,
) -> Result<ManifestContext, KainError> {
    let root_dir = find_kain_manifest_root(start_dir);
    let config = root_dir
        .as_ref()
        .map(|root| load_rust_ffi_config(root.as_path()))
        .transpose()?
        .flatten();
    let manifest_path = if let Some(path) = prepare
        .manifest_path
        .as_ref()
        .or(options.manifest_path.as_ref())
    {
        Some(canonicalize_lossy(path))
    } else if let Some(config_manifest) = config
        .as_ref()
        .and_then(|value| value.manifest_path.as_ref())
        .map(|path| resolve_relative_to(path, root_dir.as_deref().unwrap_or(start_dir)))
    {
        Some(config_manifest)
    } else {
        find_nearest_cargo_manifest(start_dir)
    };

    Ok(ManifestContext {
        root_dir,
        config,
        manifest_path,
    })
}

pub fn build_cache_hash(
    resolved: &ResolvedCrate,
    source_file_hashes: &[(PathBuf, String)],
    target_triple: &str,
    rustc_version: &str,
    kain_version: &str,
    bridge_format_version: &str,
) -> String {
    let mut hasher = Sha256::new();
    hasher.update(resolved.package_name.as_bytes());
    hasher.update(resolved.import_name.as_bytes());
    hasher.update(resolved.version.as_bytes());
    hasher.update(resolved.manifest_path.to_string_lossy().as_bytes());
    hasher.update(target_triple.as_bytes());
    hasher.update(rustc_version.as_bytes());
    hasher.update(kain_version.as_bytes());
    hasher.update(bridge_format_version.as_bytes());
    for feature in &resolved.features {
        hasher.update(feature.as_bytes());
    }
    hasher.update([resolved.default_features as u8, resolved.all_features as u8]);
    for (path, digest) in source_file_hashes {
        hasher.update(path.to_string_lossy().as_bytes());
        hasher.update(digest.as_bytes());
    }
    let digest = hasher.finalize();
    format!("{:x}", digest)
}

pub fn rustc_version_and_target() -> Result<(String, String), KainError> {
    let output = Command::new("rustc")
        .arg("-vV")
        .output()
        .map_err(|err| KainError::runtime(format!("Failed to run rustc -vV: {err}")))?;
    if !output.status.success() {
        return Err(KainError::runtime(
            "Failed to read rustc version information for crate FFI cache key",
        ));
    }
    let stdout = String::from_utf8_lossy(&output.stdout);
    let version_line = stdout
        .lines()
        .find(|line| line.starts_with("release: "))
        .unwrap_or("release: unknown");
    let host_line = stdout
        .lines()
        .find(|line| line.starts_with("host: "))
        .unwrap_or("host: unknown");
    Ok((
        version_line
            .trim_start_matches("release: ")
            .trim()
            .to_string(),
        host_line.trim_start_matches("host: ").trim().to_string(),
    ))
}

fn resolve_from_workspace_or_dependencies(
    crate_name: &str,
    manifest_path: &Path,
    metadata: &CargoMetadata,
    options: &ImportCrateOptions,
) -> Result<Option<ResolvedCrate>, KainError> {
    let workspace_members = metadata.workspace_members.iter().collect::<HashSet<_>>();
    let workspace_match = metadata.packages.iter().find(|package| {
        workspace_members.contains(&package.id) && package_matches_import_name(package, crate_name)
    });
    if let Some(package) = workspace_match {
        return Ok(Some(build_resolved_crate(
            crate_name,
            package,
            metadata,
            ResolutionKind::Workspace,
            options,
        )?));
    }

    let dependency_match = metadata
        .packages
        .iter()
        .find(|package| package_matches_import_name(package, crate_name));
    if let Some(package) = dependency_match {
        return Ok(Some(build_resolved_crate(
            crate_name,
            package,
            metadata,
            ResolutionKind::Dependency,
            options,
        )?));
    }

    let manifest_dir = manifest_path.parent().unwrap_or_else(|| Path::new("."));
    let searched_workspace = manifest_dir.join(CARGO_MANIFEST_NAME);
    if searched_workspace.exists() {
        return Ok(None);
    }

    Ok(None)
}

fn resolve_explicit_crate_path(
    crate_name: &str,
    crate_path: &Path,
    options: &ImportCrateOptions,
) -> Result<ResolvedCrate, KainError> {
    let crate_dir = if crate_path.is_dir() {
        crate_path.to_path_buf()
    } else {
        crate_path
            .parent()
            .map(Path::to_path_buf)
            .unwrap_or_else(|| PathBuf::from("."))
    };
    let manifest_path = if crate_path.file_name() == Some(OsStr::new(CARGO_MANIFEST_NAME)) {
        crate_path.to_path_buf()
    } else {
        crate_dir.join(CARGO_MANIFEST_NAME)
    };
    if !manifest_path.exists() {
        return Err(KainError::runtime(format!(
            "--crate-path '{}' does not contain a Cargo.toml",
            crate_path.display()
        )));
    }

    let metadata = load_metadata(&manifest_path, options)?;
    let root_package = metadata
        .packages
        .iter()
        .find(|package| canonicalize_lossy(Path::new(&package.manifest_path)) == manifest_path)
        .or_else(|| metadata.packages.first())
        .ok_or_else(|| {
            KainError::runtime(format!(
                "Cargo metadata for '{}' did not include a root package",
                manifest_path.display()
            ))
        })?;

    build_resolved_crate(
        crate_name,
        root_package,
        &metadata,
        ResolutionKind::CratePath,
        options,
    )
}

fn resolve_config_path_crate(
    crate_name: &str,
    entry: &RustFfiPathCrate,
    manifest_context: &ManifestContext,
    options: &ImportCrateOptions,
) -> Result<ResolvedCrate, KainError> {
    let root = manifest_context
        .root_dir
        .as_deref()
        .unwrap_or_else(|| Path::new("."));
    let crate_dir = resolve_relative_to(&entry.path, root);
    resolve_explicit_crate_path(crate_name, &crate_dir, options).map(|mut resolved| {
        resolved.resolution_kind = ResolutionKind::PathConfig;
        resolved
    })
}

fn resolve_registry_crate(
    crate_name: &str,
    entry: &RustFfiRegistryCrate,
    options: &ImportCrateOptions,
) -> Result<ResolvedCrate, KainError> {
    let resolution_root = std::env::temp_dir()
        .join("kain_crate_ffi_registry_resolution")
        .join(format!(
            "{}_{}",
            crate_name,
            sanitize_for_filename(&entry.version)
        ));
    kfs::create_dir_all(&resolution_root).map_err(fs_to_kain_error)?;
    let dependency_key = crate_name.replace('-', "_");
    let package_name = entry
        .package
        .clone()
        .unwrap_or_else(|| crate_name.to_string());
    let manifest_path = resolution_root.join(CARGO_MANIFEST_NAME);

    let mut dependency_lines = Vec::new();
    dependency_lines.push(format!(
        "{dependency_key} = {{ package = {:?}, version = {:?}",
        package_name, entry.version
    ));
    if !entry.features.is_empty() {
        dependency_lines.push(format!(", features = {:?}", entry.features));
    }
    if !entry.default_features {
        dependency_lines.push(", default-features = false".to_string());
    }
    dependency_lines.push(" }".to_string());

    let manifest = format!(
        "[package]\nname = \"kain-crate-ffi-resolve-{dependency_key}\"\nversion = \"0.1.0\"\nedition = \"2021\"\n\n[dependencies]\n{}\n",
        dependency_lines.join("")
    );
    kfs::atomic_write_text(&manifest_path, &manifest).map_err(fs_to_kain_error)?;
    let metadata = load_metadata(&manifest_path, options)?;
    let package = metadata
        .packages
        .iter()
        .find(|package| {
            package.name == package_name || package_matches_import_name(package, crate_name)
        })
        .ok_or_else(|| {
            KainError::runtime(format!(
                "Cargo metadata could not resolve registry crate '{crate_name}' from version '{}'",
                entry.version
            ))
        })?;
    let mut resolved = build_resolved_crate(
        crate_name,
        package,
        &metadata,
        ResolutionKind::RegistryConfig,
        &ImportCrateOptions {
            features: entry.features.clone(),
            no_default_features: !entry.default_features,
            ..options.clone()
        },
    )?;
    resolved.dependency_spec = DependencySpec::Registry {
        package: package.name.clone(),
        dependency_name: resolved.dependency_name.clone(),
        version: package.version.to_string(),
        features: entry.features.clone(),
        default_features: entry.default_features,
    };
    resolved.default_features = entry.default_features;
    resolved.features = entry.features.clone();
    Ok(resolved)
}

fn build_resolved_crate(
    crate_name: &str,
    package: &CargoPackage,
    metadata: &CargoMetadata,
    resolution_kind: ResolutionKind,
    options: &ImportCrateOptions,
) -> Result<ResolvedCrate, KainError> {
    let lib_target = package
        .targets
        .iter()
        .find(|target| target_is_library(target))
        .or_else(|| package.targets.first())
        .ok_or_else(|| {
            KainError::runtime(format!(
                "Rust crate '{}' does not expose a target that Kain crate FFI can inspect",
                package.name
            ))
        })?;

    let crate_root_file = canonicalize_lossy(Path::new(&lib_target.src_path));
    let crate_root = crate_root_file
        .parent()
        .map(Path::to_path_buf)
        .unwrap_or_else(|| PathBuf::from("."));
    let manifest_path = canonicalize_lossy(Path::new(&package.manifest_path));
    let dependency_name = lib_target.name.replace('-', "_");
    let dependency_spec = match resolution_kind {
        ResolutionKind::RegistryConfig => DependencySpec::Registry {
            package: package.name.clone(),
            dependency_name: dependency_name.clone(),
            version: package.version.to_string(),
            features: options.features.clone(),
            default_features: !options.no_default_features,
        },
        _ => DependencySpec::Path {
            package: package.name.clone(),
            dependency_name: dependency_name.clone(),
            path: manifest_path
                .parent()
                .map(Path::to_path_buf)
                .unwrap_or_else(|| crate_root.clone()),
        },
    };
    let feature_set = normalized_feature_set(options);

    Ok(ResolvedCrate {
        import_name: crate_name.to_string(),
        package_name: package.name.clone(),
        dependency_name,
        library_target_name: lib_target.name.clone(),
        version: package.version.clone(),
        manifest_path,
        crate_root,
        crate_root_file,
        workspace_root: canonicalize_lossy(Path::new(&metadata.workspace_root)),
        resolution_kind,
        dependency_spec,
        features: feature_set,
        default_features: !options.no_default_features,
        all_features: options.all_features,
    })
}

fn package_matches_import_name(package: &CargoPackage, crate_name: &str) -> bool {
    if package.name == crate_name {
        return true;
    }
    package.targets.iter().any(|target| {
        target
            .kind
            .iter()
            .any(|kind| kind == "lib" || kind == "rlib" || kind == "cdylib")
            && target.name == crate_name
    })
}

fn normalized_feature_set(options: &ImportCrateOptions) -> Vec<String> {
    let mut features = BTreeSet::new();
    for feature in &options.features {
        let trimmed = feature.trim();
        if !trimmed.is_empty() {
            features.insert(trimmed.to_string());
        }
    }
    features.into_iter().collect()
}

fn load_metadata(
    manifest_path: &Path,
    options: &ImportCrateOptions,
) -> Result<CargoMetadata, KainError> {
    let mut command = Command::new("cargo");
    command
        .arg("metadata")
        .arg("--format-version")
        .arg("1")
        .arg("--manifest-path")
        .arg(manifest_path);
    if options.all_features {
        command.arg("--all-features");
    } else {
        if options.no_default_features {
            command.arg("--no-default-features");
        }
        if !options.features.is_empty() {
            command.arg("--features");
            command.arg(options.features.join(","));
        }
    }
    let output = command.output().map_err(|err| {
        KainError::runtime(format!(
            "Failed to run cargo metadata for '{}': {err}",
            manifest_path.display()
        ))
    })?;
    if !output.status.success() {
        return Err(KainError::runtime(format!(
            "cargo metadata failed for '{}': {}",
            manifest_path.display(),
            String::from_utf8_lossy(&output.stderr)
        )));
    }
    serde_json::from_slice(&output.stdout).map_err(|err| {
        KainError::runtime(format!(
            "Failed to parse cargo metadata JSON for '{}': {err}",
            manifest_path.display()
        ))
    })
}

fn load_rust_ffi_config(root: &Path) -> Result<Option<RustFfiConfig>, KainError> {
    let manifest_path = KAIN_MANIFEST_NAMES
        .iter()
        .map(|name| root.join(name))
        .find(|path| path.exists());
    let Some(manifest_path) = manifest_path else {
        return Ok(None);
    };
    let source = kfs::read_text(&manifest_path).map_err(fs_to_kain_error)?;
    let value = source.parse::<toml::Value>().map_err(|err| {
        KainError::runtime(format!(
            "Failed to parse KAIN manifest '{}': {err}",
            manifest_path.display()
        ))
    })?;
    let section = value
        .get("rust_ffi")
        .cloned()
        .unwrap_or_else(|| toml::Value::Table(Default::default()));
    if section.as_table().is_some_and(|table| table.is_empty()) {
        return Ok(None);
    }
    let config = section.try_into::<RustFfiConfig>().map_err(|err| {
        KainError::runtime(format!(
            "Failed to parse [rust_ffi] in '{}': {err}",
            manifest_path.display()
        ))
    })?;
    Ok(Some(config))
}

fn find_kain_manifest_root(start_dir: &Path) -> Option<PathBuf> {
    start_dir
        .ancestors()
        .find(|dir| {
            KAIN_MANIFEST_NAMES
                .iter()
                .any(|name| dir.join(name).exists())
        })
        .map(Path::to_path_buf)
}

fn find_nearest_cargo_manifest(start_dir: &Path) -> Option<PathBuf> {
    start_dir
        .ancestors()
        .map(|dir| dir.join(CARGO_MANIFEST_NAME))
        .find(|path| path.exists())
        .map(|path| canonicalize_lossy(&path))
}

fn resolve_relative_to(path: &Path, base_dir: &Path) -> PathBuf {
    if path.is_absolute() {
        canonicalize_lossy(path)
    } else {
        canonicalize_lossy(&base_dir.join(path))
    }
}

fn sanitize_for_filename(value: &str) -> String {
    value
        .chars()
        .map(|ch| if ch.is_ascii_alphanumeric() { ch } else { '_' })
        .collect()
}

fn canonicalize_lossy(path: &Path) -> PathBuf {
    kfs::canonicalize_path(path)
        .map(PathBuf::from)
        .unwrap_or_else(|_| path.to_path_buf())
}

pub fn simple_file_sha256(path: &Path) -> Result<String, KainError> {
    kfs::hash_file(path).map_err(fs_to_kain_error)
}

pub fn lib_filename(stem: &str) -> String {
    if cfg!(target_os = "windows") {
        format!("{stem}.dll")
    } else if cfg!(target_os = "macos") {
        format!("lib{stem}.dylib")
    } else {
        format!("lib{stem}.so")
    }
}

pub fn target_directory_name() -> &'static str {
    "debug"
}

fn fs_to_kain_error(error: kain_fs::FsError) -> KainError {
    KainError::runtime(format!("Filesystem error: {error}"))
}

pub fn build_cache_inputs(files: &[PathBuf]) -> Result<CacheInputs, KainError> {
    let (rustc_version, target_triple) = rustc_version_and_target()?;
    let mut source_file_hashes = Vec::new();
    for file in files {
        source_file_hashes.push((file.clone(), simple_file_sha256(file)?));
    }
    Ok(CacheInputs {
        target_triple,
        rustc_version,
        source_file_hashes,
    })
}

fn target_is_library(target: &CargoTarget) -> bool {
    target
        .kind
        .iter()
        .any(|kind| kind == "lib" || kind == "rlib" || kind == "cdylib")
}
