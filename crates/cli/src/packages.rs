use blade::{
    package_capsule_path, package_index_path, package_install_metadata_path, package_version_root,
    package_workspace_root, InstalledPackageIndex, InstalledPackageVersion, LockedPackage,
    PackageLockfile, KAIN_LOCKFILE_NAME,
};
use chrono::Utc;
use kain_amalgamate::{
    inspect_capsule, maybe_capsule_metadata, pack_capsule, unpack_capsule, CapsuleContents,
    CapsuleStorage, PackOptions,
};
use kain_core::install_layout::{default_kain_install_layout, KainInstallLayout};
use serde::{Deserialize, Serialize};
use std::fs;
use std::path::{Path, PathBuf};

const PACKAGE_LOCK_SCHEMA_VERSION: u32 = 1;
const INSTALLED_PACKAGE_INDEX_SCHEMA_VERSION: u32 = 1;
const PACKAGE_INSTALL_METADATA_SCHEMA_VERSION: u32 = 1;

#[derive(Debug, Clone)]
pub struct PublishReport {
    pub name: String,
    pub version: String,
    pub source_capsule: PathBuf,
    pub artifact_capsule: Option<PathBuf>,
    pub evidence_capsule: Option<PathBuf>,
}

#[derive(Debug, Clone)]
pub struct InstallReport {
    pub name: String,
    pub version: String,
    pub package_root: PathBuf,
    pub workspace_root: PathBuf,
    pub source_capsule: PathBuf,
}

#[derive(Debug, Clone)]
pub struct AddReport {
    pub package_name: String,
    pub version: String,
    pub manifest_path: PathBuf,
    pub lockfile_path: PathBuf,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
struct InstalledPackageMetadata {
    schema: u32,
    name: String,
    version: String,
    digest: String,
    kind: String,
    capsule_set: Option<String>,
    installed_at: String,
    source_capsule: String,
    companion_capsules: Vec<String>,
}

pub fn publish(
    input: &Path,
    output: Option<&Path>,
    name_override: Option<String>,
    version_override: Option<String>,
    include_artifacts: bool,
    include_evidence: bool,
    archive: bool,
) -> Result<PublishReport, String> {
    if !input.exists() {
        return Err(format!(
            "publish input '{}' does not exist",
            input.display()
        ));
    }
    let input = canonicalize_lossy(input);
    let package_name = name_override.unwrap_or_else(|| infer_package_name(&input));
    let package_version = version_override
        .unwrap_or_else(|| infer_package_version(&input).unwrap_or_else(|| "0.1.0".to_string()));
    let source_output = output
        .map(PathBuf::from)
        .unwrap_or_else(|| default_publish_output_path(&input, &package_name, &package_version));
    let source_output = ensure_parent_directory(source_output)?;

    let source_report = pack_package_capsule(
        &input,
        &source_output,
        &package_name,
        &package_version,
        CapsuleContents::Source,
        archive,
    )?;

    let artifact_capsule = if include_artifacts {
        let path = companion_output_path(&source_output, "artifacts");
        pack_package_capsule(
            &input,
            &path,
            &package_name,
            &package_version,
            CapsuleContents::Artifacts,
            archive,
        )?;
        Some(path)
    } else {
        None
    };

    let evidence_capsule = if include_evidence {
        let path = companion_output_path(&source_output, "evidence");
        pack_package_capsule(
            &input,
            &path,
            &package_name,
            &package_version,
            CapsuleContents::Evidence,
            archive,
        )?;
        Some(path)
    } else {
        None
    };

    Ok(PublishReport {
        name: source_report.name,
        version: package_version,
        source_capsule: source_report.output_path,
        artifact_capsule,
        evidence_capsule,
    })
}

pub fn install(spec: &str, version_override: Option<String>) -> Result<InstallReport, String> {
    let layout = require_install_layout()?;
    fs::create_dir_all(&layout.packages_dir).map_err(|err| {
        format!(
            "failed to create package store '{}': {err}",
            layout.packages_dir.display()
        )
    })?;

    if let Some(existing) = resolve_existing_package(spec, version_override.as_deref(), &layout)? {
        activate_installed_version(&existing.package_root, &existing.version)?;
        return Ok(existing);
    }

    let input_path = PathBuf::from(spec);
    let install_input = if input_path.exists() {
        input_path
    } else {
        return Err(format!(
            "package '{}' is not installed and was not found as a local path or capsule",
            spec
        ));
    };

    if install_input.is_dir() {
        let published = publish(
            &install_input,
            None,
            None,
            version_override.clone(),
            false,
            false,
            false,
        )?;
        install_from_source_capsule(&published.source_capsule, version_override, &layout)
    } else {
        install_from_source_capsule(&install_input, version_override, &layout)
    }
}

pub fn add(
    spec: &str,
    version_override: Option<String>,
    manifest_override: Option<&Path>,
) -> Result<AddReport, String> {
    let install = install(spec, version_override)?;
    let manifest_root = resolve_manifest_root(manifest_override)?;
    let manifest_path = manifest_root.join("KAIN.toml");
    let lockfile_path = manifest_root.join(KAIN_LOCKFILE_NAME);

    update_project_manifest(&manifest_path, &install.name, &install.version)?;
    update_lockfile(
        &lockfile_path,
        &LockedPackage {
            name: install.name.clone(),
            version: install.version.clone(),
            source: "kain_home".to_string(),
            digest: load_installed_digest(&install.package_root, &install.version)?,
            kind: "blade".to_string(),
            capsule_set: Some(install.name.clone()),
        },
    )?;

    Ok(AddReport {
        package_name: install.name,
        version: install.version,
        manifest_path,
        lockfile_path,
    })
}

fn install_from_source_capsule(
    source_capsule: &Path,
    version_override: Option<String>,
    layout: &KainInstallLayout,
) -> Result<InstallReport, String> {
    let metadata = maybe_capsule_metadata(source_capsule)
        .map_err(|err| err.to_string())?
        .ok_or_else(|| format!("'{}' is not a Kain capsule", source_capsule.display()))?;
    if metadata.contents != CapsuleContents::Source {
        return Err(format!(
            "package install expects a source capsule, but '{}' is '{}'",
            source_capsule.display(),
            metadata.contents
        ));
    }

    let inspect = inspect_capsule(source_capsule).map_err(|err| err.to_string())?;
    let package_name = metadata
        .name
        .clone()
        .or_else(|| metadata.root_label.clone())
        .unwrap_or_else(|| fallback_name_from_path(source_capsule));
    let package_version = version_override
        .or_else(|| metadata.version.clone())
        .unwrap_or_else(|| "0.1.0".to_string());
    let package_root = layout
        .packages_dir
        .join(normalize_package_key(&package_name));
    let version_root = package_version_root(&package_root, &package_version);
    let workspace_root = package_workspace_root(&package_root, &package_version);

    if version_root.exists() {
        fs::remove_dir_all(&version_root)
            .map_err(|err| format!("failed to remove '{}': {err}", version_root.display()))?;
    }
    fs::create_dir_all(&version_root).map_err(|err| {
        format!(
            "failed to create version directory '{}': {err}",
            version_root.display()
        )
    })?;

    let installed_source = package_capsule_path(&package_root, &package_version, "source");
    copy_file(source_capsule, &installed_source)?;

    let companion_capsules =
        discover_companion_capsules(source_capsule, metadata.capsule_set.as_deref())?;
    let mut installed_companions = Vec::new();
    for companion in companion_capsules {
        let companion_metadata = maybe_capsule_metadata(&companion)
            .map_err(|err| err.to_string())?
            .ok_or_else(|| {
                format!(
                    "failed to inspect companion capsule '{}'",
                    companion.display()
                )
            })?;
        let installed_companion = package_capsule_path(
            &package_root,
            &package_version,
            &companion_metadata.contents.to_string(),
        );
        copy_file(&companion, &installed_companion)?;
        installed_companions.push(installed_companion);
    }

    unpack_capsule(&installed_source, &workspace_root).map_err(|err| err.to_string())?;

    let mut index = load_or_default_package_index(&package_root)?;
    index.schema = INSTALLED_PACKAGE_INDEX_SCHEMA_VERSION;
    index.name = package_name.clone();
    index.active_version = Some(package_version.clone());
    index
        .versions
        .retain(|entry| entry.version != package_version);
    index.versions.push(InstalledPackageVersion {
        version: package_version.clone(),
        digest: inspect.metadata.digest.clone(),
        kind: inspect.metadata.display_kind().to_string(),
        contents: build_contents_list(&installed_companions),
        capsule_set: metadata.capsule_set.clone(),
    });
    index
        .versions
        .sort_by(|left, right| left.version.cmp(&right.version));
    write_package_index(&package_root, &index)?;

    let install_metadata = InstalledPackageMetadata {
        schema: PACKAGE_INSTALL_METADATA_SCHEMA_VERSION,
        name: package_name.clone(),
        version: package_version.clone(),
        digest: inspect.metadata.digest.clone(),
        kind: inspect.metadata.display_kind().to_string(),
        capsule_set: metadata.capsule_set.clone(),
        installed_at: Utc::now().to_rfc3339(),
        source_capsule: installed_source
            .file_name()
            .and_then(|value| value.to_str())
            .unwrap_or("source.kn")
            .to_string(),
        companion_capsules: installed_companions
            .iter()
            .filter_map(|path| {
                path.file_name()
                    .and_then(|value| value.to_str())
                    .map(|value| value.to_string())
            })
            .collect(),
    };
    write_text_file(
        package_install_metadata_path(&package_root, &package_version),
        serde_json::to_string_pretty(&install_metadata)
            .map_err(|err| format!("failed to serialize install metadata: {err}"))?,
    )?;

    Ok(InstallReport {
        name: package_name,
        version: package_version,
        package_root,
        workspace_root,
        source_capsule: installed_source,
    })
}

fn resolve_existing_package(
    spec: &str,
    version: Option<&str>,
    layout: &KainInstallLayout,
) -> Result<Option<InstallReport>, String> {
    let store_root = &layout.packages_dir;
    if !store_root.exists() {
        return Ok(None);
    }
    for entry in fs::read_dir(store_root).map_err(|err| {
        format!(
            "failed to read package store '{}': {err}",
            store_root.display()
        )
    })? {
        let entry = entry.map_err(|err| format!("failed to inspect package store entry: {err}"))?;
        if !entry.file_type().map_err(|err| err.to_string())?.is_dir() {
            continue;
        }
        let package_root = entry.path();
        let Some(index) = load_package_index_from_store(&package_root)? else {
            continue;
        };
        if !names_match(&index.name, spec)
            && !package_root
                .file_name()
                .and_then(|value| value.to_str())
                .is_some_and(|value| names_match(value, spec))
        {
            continue;
        }
        let selected = select_installed_version(&index, version).ok_or_else(|| {
            format!(
                "package '{}' does not have version '{}'",
                spec,
                version.unwrap_or_default()
            )
        })?;
        let package_version = selected.version.clone();
        let workspace_root = package_workspace_root(&package_root, &package_version);
        let source_capsule = package_capsule_path(&package_root, &package_version, "source");
        return Ok(Some(InstallReport {
            name: index.name.clone(),
            version: package_version,
            package_root,
            workspace_root,
            source_capsule,
        }));
    }
    Ok(None)
}

fn activate_installed_version(package_root: &Path, version: &str) -> Result<(), String> {
    let Some(mut index) = load_package_index_from_store(package_root)? else {
        return Ok(());
    };
    index.active_version = Some(version.to_string());
    write_package_index(package_root, &index)
}

fn load_or_default_package_index(package_root: &Path) -> Result<InstalledPackageIndex, String> {
    Ok(load_package_index_from_store(package_root)?.unwrap_or_default())
}

fn load_package_index_from_store(
    package_root: &Path,
) -> Result<Option<InstalledPackageIndex>, String> {
    blade::load_installed_package_index(package_root).map_err(|err| err.to_string())
}

fn write_package_index(package_root: &Path, index: &InstalledPackageIndex) -> Result<(), String> {
    fs::create_dir_all(package_root).map_err(|err| {
        format!(
            "failed to create package root '{}': {err}",
            package_root.display()
        )
    })?;
    write_text_file(
        package_index_path(package_root),
        serde_json::to_string_pretty(index)
            .map_err(|err| format!("failed to serialize installed package index: {err}"))?,
    )
}

fn update_project_manifest(
    manifest_path: &Path,
    package_name: &str,
    version: &str,
) -> Result<(), String> {
    let mut root = if manifest_path.exists() {
        let source = read_text_file(manifest_path)?;
        toml::from_str::<toml::Value>(&source)
            .map_err(|err| format!("failed to parse '{}': {err}", manifest_path.display()))?
    } else {
        toml::Value::Table(Default::default())
    };

    let table = root
        .as_table_mut()
        .ok_or_else(|| format!("'{}' must be a TOML table", manifest_path.display()))?;
    if !table.contains_key("package") {
        table.insert(
            "package".to_string(),
            toml::Value::Table(Default::default()),
        );
    }
    if !table.contains_key("blade") {
        table.insert("blade".to_string(), toml::Value::Table(Default::default()));
    }

    let blade = table
        .get_mut("blade")
        .and_then(toml::Value::as_table_mut)
        .ok_or_else(|| {
            format!(
                "'{}' [blade] section must be a TOML table",
                manifest_path.display()
            )
        })?;
    if !blade.contains_key("dependencies") {
        blade.insert("dependencies".to_string(), toml::Value::Array(Vec::new()));
    }
    let dependencies = blade
        .get_mut("dependencies")
        .and_then(toml::Value::as_array_mut)
        .ok_or_else(|| {
            format!(
                "'{}' blade.dependencies must be an array of tables",
                manifest_path.display()
            )
        })?;

    let mut updated = false;
    for dependency in dependencies.iter_mut() {
        let Some(table) = dependency.as_table_mut() else {
            continue;
        };
        let Some(existing_name) = table.get("name").and_then(toml::Value::as_str) else {
            continue;
        };
        if !names_match(existing_name, package_name) {
            continue;
        }
        table.insert(
            "version".to_string(),
            toml::Value::String(version.to_string()),
        );
        updated = true;
        break;
    }

    if !updated {
        let mut dependency = toml::map::Map::new();
        dependency.insert(
            "name".to_string(),
            toml::Value::String(package_name.to_string()),
        );
        dependency.insert(
            "version".to_string(),
            toml::Value::String(version.to_string()),
        );
        dependencies.push(toml::Value::Table(dependency));
    }

    ensure_manifest_parent(manifest_path)?;
    write_text_file(
        manifest_path,
        toml::to_string_pretty(&root)
            .map_err(|err| format!("failed to serialize manifest TOML: {err}"))?,
    )
}

fn update_lockfile(lockfile_path: &Path, package: &LockedPackage) -> Result<(), String> {
    let mut lockfile = if lockfile_path.exists() {
        let source = read_text_file(lockfile_path)?;
        toml::from_str::<PackageLockfile>(&source)
            .map_err(|err| format!("failed to parse '{}': {err}", lockfile_path.display()))?
    } else {
        PackageLockfile::default()
    };
    lockfile.schema = PACKAGE_LOCK_SCHEMA_VERSION;
    lockfile
        .packages
        .retain(|existing| !names_match(&existing.name, &package.name));
    lockfile.packages.push(package.clone());
    lockfile.packages.sort_by(|left, right| {
        normalize_package_key(&left.name).cmp(&normalize_package_key(&right.name))
    });

    ensure_manifest_parent(lockfile_path)?;
    write_text_file(
        lockfile_path,
        toml::to_string_pretty(&lockfile)
            .map_err(|err| format!("failed to serialize lockfile TOML: {err}"))?,
    )
}

fn resolve_manifest_root(manifest_override: Option<&Path>) -> Result<PathBuf, String> {
    if let Some(path) = manifest_override {
        if path
            .file_name()
            .and_then(|value| value.to_str())
            .is_some_and(|value| value.eq_ignore_ascii_case("KAIN.toml"))
        {
            return Ok(path
                .parent()
                .map(canonicalize_lossy)
                .unwrap_or_else(|| PathBuf::from(".")));
        }
        return Ok(canonicalize_lossy(path));
    }
    let cwd = std::env::current_dir()
        .map_err(|err| format!("failed to inspect current directory: {err}"))?;
    Ok(blade::discover_workspace_root(&cwd).unwrap_or_else(|_| canonicalize_lossy(&cwd)))
}

fn require_install_layout() -> Result<KainInstallLayout, String> {
    default_kain_install_layout().ok_or_else(|| "failed to resolve Kain install layout".to_string())
}

fn pack_package_capsule(
    input: &Path,
    output: &Path,
    name: &str,
    version: &str,
    contents: CapsuleContents,
    archive: bool,
) -> Result<kain_amalgamate::PackReport, String> {
    let mut options = PackOptions::new(input, output);
    options.name = Some(name.to_string());
    options.version = Some(version.to_string());
    options.capsule_set = Some(name.to_string());
    options.contents = contents;
    options.storage = if archive {
        CapsuleStorage::Archive
    } else {
        CapsuleStorage::Editable
    };
    pack_capsule(&options).map_err(|err| err.to_string())
}

fn discover_companion_capsules(
    source_capsule: &Path,
    capsule_set: Option<&str>,
) -> Result<Vec<PathBuf>, String> {
    let Some(capsule_set) = capsule_set else {
        return Ok(Vec::new());
    };
    let Some(parent) = source_capsule.parent() else {
        return Ok(Vec::new());
    };
    let mut companions = Vec::new();
    for entry in fs::read_dir(parent).map_err(|err| {
        format!(
            "failed to scan companion capsules in '{}': {err}",
            parent.display()
        )
    })? {
        let entry = entry.map_err(|err| err.to_string())?;
        let path = entry.path();
        if path == source_capsule || path.extension().and_then(|value| value.to_str()) != Some("kn")
        {
            continue;
        }
        let Some(metadata) = maybe_capsule_metadata(&path).map_err(|err| err.to_string())? else {
            continue;
        };
        if metadata.capsule_set.as_deref() != Some(capsule_set) {
            continue;
        }
        if matches!(
            metadata.contents,
            CapsuleContents::Source | CapsuleContents::Snapshot
        ) {
            continue;
        }
        companions.push(path);
    }
    companions.sort();
    Ok(companions)
}

fn load_installed_digest(package_root: &Path, version: &str) -> Result<String, String> {
    let Some(index) = load_package_index_from_store(package_root)? else {
        return Err(format!(
            "installed package metadata missing for '{}'",
            package_root.display()
        ));
    };
    let selected = select_installed_version(&index, Some(version)).ok_or_else(|| {
        format!(
            "installed package '{}' is missing version '{}'",
            index.name, version
        )
    })?;
    Ok(selected.digest.clone())
}

fn select_installed_version<'a>(
    index: &'a InstalledPackageIndex,
    version: Option<&str>,
) -> Option<&'a InstalledPackageVersion> {
    if let Some(version) = version {
        return index
            .versions
            .iter()
            .find(|candidate| candidate.version == version);
    }
    if let Some(active_version) = index.active_version.as_deref() {
        if let Some(selected) = index
            .versions
            .iter()
            .find(|candidate| candidate.version == active_version)
        {
            return Some(selected);
        }
    }
    index
        .versions
        .iter()
        .max_by(|left, right| left.version.cmp(&right.version))
}

fn infer_package_name(input: &Path) -> String {
    blade::load_effective_kain_manifest(input)
        .ok()
        .flatten()
        .and_then(|manifest| manifest.package.name.or(manifest.blade.name))
        .unwrap_or_else(|| fallback_name_from_path(input))
}

fn infer_package_version(input: &Path) -> Option<String> {
    blade::load_effective_kain_manifest(input)
        .ok()
        .flatten()
        .and_then(|manifest| manifest.package.version.or(manifest.blade.version))
}

fn default_publish_output_path(input: &Path, name: &str, version: &str) -> PathBuf {
    let base = if input.is_dir() {
        input.to_path_buf()
    } else {
        input
            .parent()
            .map(PathBuf::from)
            .unwrap_or_else(|| PathBuf::from("."))
    };
    base.join(".kain")
        .join("publish")
        .join(format!("{name}-{version}.kn"))
}

fn companion_output_path(source_output: &Path, suffix: &str) -> PathBuf {
    let stem = source_output
        .file_stem()
        .and_then(|value| value.to_str())
        .unwrap_or("package");
    let file_name = format!("{stem}.{suffix}.kn");
    source_output
        .parent()
        .unwrap_or_else(|| Path::new("."))
        .join(file_name)
}

fn ensure_parent_directory(path: PathBuf) -> Result<PathBuf, String> {
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent)
            .map_err(|err| format!("failed to create '{}': {err}", parent.display()))?;
    }
    Ok(path)
}

fn ensure_manifest_parent(path: &Path) -> Result<(), String> {
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent)
            .map_err(|err| format!("failed to create '{}': {err}", parent.display()))?;
    }
    Ok(())
}

fn copy_file(from: &Path, to: &Path) -> Result<(), String> {
    if let Some(parent) = to.parent() {
        fs::create_dir_all(parent)
            .map_err(|err| format!("failed to create '{}': {err}", parent.display()))?;
    }
    fs::copy(from, to).map_err(|err| {
        format!(
            "failed to copy '{}' to '{}': {err}",
            from.display(),
            to.display()
        )
    })?;
    Ok(())
}

fn build_contents_list(companion_capsules: &[PathBuf]) -> Vec<String> {
    let mut contents = vec!["source".to_string()];
    for path in companion_capsules {
        if let Some(stem) = path.file_stem().and_then(|value| value.to_str()) {
            contents.push(stem.to_string());
        }
    }
    contents
}

fn normalize_package_key(value: &str) -> String {
    value.trim().replace('_', "-").to_ascii_lowercase()
}

fn names_match(left: &str, right: &str) -> bool {
    left == right || normalize_package_key(left) == normalize_package_key(right)
}

fn fallback_name_from_path(path: &Path) -> String {
    path.file_stem()
        .or_else(|| path.file_name())
        .and_then(|value| value.to_str())
        .filter(|value| !value.trim().is_empty())
        .unwrap_or("kain-package")
        .to_string()
}

fn canonicalize_lossy(path: &Path) -> PathBuf {
    fs::canonicalize(path).unwrap_or_else(|_| path.to_path_buf())
}

fn read_text_file(path: &Path) -> Result<String, String> {
    fs::read_to_string(path).map_err(|err| format!("failed to read '{}': {err}", path.display()))
}

fn write_text_file(path: impl AsRef<Path>, content: String) -> Result<(), String> {
    let path = path.as_ref();
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent)
            .map_err(|err| format!("failed to create '{}': {err}", parent.display()))?;
    }
    fs::write(path, content).map_err(|err| format!("failed to write '{}': {err}", path.display()))
}

pub fn install_c_extras(
    _packages: &[String],
    _target: &str,
    dry_run: bool,
) -> Result<(), String> {
    if dry_run {
        println!("Would bootstrap vcpkg into ~/.kain/vcpkg/");
        return Ok(());
    }
    kain_c_ffi::vcpkg_setup::setup_vcpkg(&kain_c_ffi::vcpkg_setup::default_vcpkg_root())
        .map_err(|e| format!("vcpkg setup failed: {e}"))
}

/// Resolve a short target name to its canonical target triple and sysroot directory name.
///
/// Supported short names:
/// - `"linux"` → `"x86_64-unknown-linux-musl"`
/// - `"linux-aarch64"` → `"aarch64-unknown-linux-musl"`
fn resolve_sysroot_target(
    target: &str,
    triple_override: Option<&str>,
) -> Result<(kain_target::TargetTriple, String), String> {
    if let Some(triple_str) = triple_override {
        let triple = kain_target::TargetTriple::parse(triple_str)
            .map_err(|e| format!("invalid --triple '{}': {}", triple_str, e))?;
        let name = triple
            .sysroot_name()
            .unwrap_or_else(|| format!("{}-{}-{}", triple.os, triple.arch, triple.env));
        return Ok((triple, name));
    }

    match target {
        "linux" => {
            let triple =
                kain_target::TargetTriple::parse("x86_64-unknown-linux-musl").unwrap();
            Ok((triple, "linux-x86_64-musl".to_string()))
        }
        "linux-aarch64" => {
            let triple =
                kain_target::TargetTriple::parse("aarch64-unknown-linux-musl").unwrap();
            Ok((triple, "linux-aarch64-musl".to_string()))
        }
        other => Err(format!(
            "unknown target '{}'. Supported: linux, linux-aarch64. Use --triple for custom targets.",
            other
        )),
    }
}

/// Return the default musl.cc cross-compiler tarball URL for a known triple.
fn muslcc_download_url(triple: &kain_target::TargetTriple) -> Option<&'static str> {
    match triple.raw().as_str() {
        "x86_64-unknown-linux-musl" => Some("https://musl.cc/x86_64-linux-musl-cross.tgz"),
        "aarch64-unknown-linux-musl" => Some("https://musl.cc/aarch64-linux-musl-cross.tgz"),
        _ => None,
    }
}

/// Install a cross-compilation sysroot into ~/.kain/sysroot/<name>/.
///
/// Downloads a prebuilt musl.cc cross toolchain tarball, extracts it, and places
/// the sysroot (headers + CRT objects + libc.a) into the Kain home directory so
/// that `kain build --target-triple` can find them for cross-compilation.
pub fn install_sysroot(
    target: &str,
    triple_override: Option<&str>,
    from_url: Option<&str>,
    from_dir: Option<&str>,
    force: bool,
    dry_run: bool,
) -> Result<(), String> {
    // --- 1. Resolve the target triple and sysroot name ---
    let (triple, sysroot_name) = resolve_sysroot_target(target, triple_override)?;

    // --- 2. Determine the install path ---
    let kain_home = default_kain_install_layout()
        .ok_or_else(|| "KAIN_HOME not found; set KAIN_HOME or run from a kain workspace".to_string())?;
    let sysroot_dir = kain_home.home_dir.join("sysroot").join(&sysroot_name);
    let metadata_path = sysroot_dir.join(".kain-sysroot.json");

    // --- 3. Check if already installed ---
    if sysroot_dir.exists() && sysroot_dir.join("lib").exists() {
        if !force {
            println!(
                "sysroot '{}' already installed at {}",
                sysroot_name,
                sysroot_dir.display()
            );
            if metadata_path.exists() {
                let meta = fs::read_to_string(&metadata_path).unwrap_or_default();
                println!("  metadata: {}", meta.trim());
            }
            println!("  (use --force to re-install)");
            return Ok(());
        }
        println!("re-installing sysroot '{}' (--force)", sysroot_name);
    }

    // --- 4. Handle dry-run ---
    if dry_run {
        println!("Would install sysroot '{}'", sysroot_name);
        println!("  target triple: {}", triple.raw());
        println!("  install path:  {}", sysroot_dir.display());
        if let Some(dir) = from_dir {
            println!("  source dir:    {}", dir);
        } else if let Some(url) = from_url {
            println!("  download URL:  {}", url);
        } else if let Some(url) = muslcc_download_url(&triple) {
            println!("  download URL:  {}", url);
        } else {
            println!("  download URL:  (no default URL for this triple; use --from-url)");
        }
        return Ok(());
    }

    // --- 5. Install from local directory ---
    if let Some(dir) = from_dir {
        let src = Path::new(dir);
        if !src.exists() {
            return Err(format!("source directory '{}' does not exist", dir));
        }
        println!("copying sysroot from '{}' to '{}'", dir, sysroot_dir.display());
        copy_dir_recursive(src, &sysroot_dir)?;
        write_sysroot_metadata(&metadata_path, &sysroot_name, &triple, None)?;
        println!("installed sysroot '{}' at {}", sysroot_name, sysroot_dir.display());
        return Ok(());
    }

    // --- 6. Download and extract ---
    let url = from_url
        .map(|u| Ok::<&str, String>(u))
        .unwrap_or_else(|| {
            muslcc_download_url(&triple)
                .ok_or_else(|| {
                    format!(
                        "no default download URL for triple '{}'. Use --from-url to specify one.",
                        triple.raw()
                    )
                })
        })?;

    println!("downloading sysroot from {}", url);
    let tmp_root = std::env::temp_dir().join(format!("kain-sysroot-{}", std::process::id()));
    fs::create_dir_all(&tmp_root)
        .map_err(|e| format!("cannot create temp dir: {}", e))?;
    let tarball_path = tmp_root.join("sysroot.tgz");

    // Download
    let response = reqwest::blocking::get(url)
        .map_err(|e| format!("download failed: {}", e))?;
    if !response.status().is_success() {
        return Err(format!("download returned HTTP {}", response.status()));
    }
    let bytes = response
        .bytes()
        .map_err(|e| format!("failed to read response body: {}", e))?;
    fs::write(&tarball_path, &bytes)
        .map_err(|e| format!("failed to write temp file: {}", e))?;

    // Extract
    println!("extracting sysroot...");
    let extract_dir = tmp_root.join("extracted");
    fs::create_dir_all(&extract_dir)
        .map_err(|e| format!("cannot create extract dir: {}", e))?;

    let tarball = fs::File::open(&tarball_path)
        .map_err(|e| format!("cannot open tarball: {}", e))?;
    let decoder = flate2::read::GzDecoder::new(tarball);
    let mut archive = tar::Archive::new(decoder);
    archive
        .unpack(&extract_dir)
        .map_err(|e| format!("extract failed: {}", e))?;

    // --- 7. Find the actual sysroot inside the extracted tree ---
    // musl.cc tarballs have a layout like:
    //   x86_64-linux-musl-cross/
    //     x86_64-linux-musl/         <-- actual sysroot
    //       lib/   (crt1.o, libc.a, ...)
    //       include/
    //     bin/   (cross compiler)
    // We want just the sysroot directory.
    let sysroot_src = find_sysroot_in_extract(&extract_dir, &triple)?;

    // --- 8. Install (move to target location) ---
    if sysroot_dir.exists() {
        fs::remove_dir_all(&sysroot_dir)
            .map_err(|e| format!("cannot remove existing sysroot: {}", e))?;
    }
    fs::create_dir_all(sysroot_dir.parent().unwrap())
        .map_err(|e| format!("cannot create sysroot parent dir: {}", e))?;

    // Copy the sysroot (not rename, since we're on a tempdir)
    copy_dir_recursive(&sysroot_src, &sysroot_dir)?;
    write_sysroot_metadata(&metadata_path, &sysroot_name, &triple, Some(url))?;

    println!(
        "installed sysroot '{}' at {}",
        sysroot_name,
        sysroot_dir.display()
    );
    Ok(())
}

/// Search the extracted tarball tree for the actual sysroot directory.
///
/// musl.cc tarballs have a top-level `<arch>-linux-musl-cross/` directory
/// that contains the sysroot at `<arch>-linux-musl/`.
fn find_sysroot_in_extract(
    extract_dir: &Path,
    triple: &kain_target::TargetTriple,
) -> Result<PathBuf, String> {
    // The musl sysroot directory name inside the tarball
    let musl_sysroot_name = format!("{}-{}-musl", triple.arch, triple.vendor);

    let result = find_dir_named(extract_dir, &musl_sysroot_name, 0);
    result.ok_or_else(|| {
        format!(
            "could not find sysroot directory '{}' inside extracted tarball at '{}'",
            musl_sysroot_name,
            extract_dir.display()
        )
    })
}

/// Recursively search for a directory with the given name, up to `depth` levels.
/// Returns the first match that contains a `lib/` subdirectory (sysroot indicator).
fn find_dir_named(root: &Path, name: &str, depth: usize) -> Option<PathBuf> {
    if depth > 3 {
        return None;
    }
    let entries = fs::read_dir(root).ok()?;
    for entry in entries.filter_map(|e| e.ok()) {
        let path = entry.path();
        if path.is_dir() {
            if entry.file_name() == name && path.join("lib").exists() {
                return Some(path);
            }
            if let Some(found) = find_dir_named(&path, name, depth + 1) {
                return Some(found);
            }
        }
    }
    None
}

/// Recursively copy a directory tree.
fn copy_dir_recursive(src: &Path, dst: &Path) -> Result<(), String> {
    fs::create_dir_all(dst)
        .map_err(|e| format!("cannot create '{}': {}", dst.display(), e))?;

    for entry in
        fs::read_dir(src).map_err(|e| format!("cannot read '{}': {}", src.display(), e))?
    {
        let entry = entry.map_err(|e| format!("cannot read entry: {}", e))?;
        let src_path = entry.path();
        let dst_path = dst.join(entry.file_name());

        if src_path.is_dir() {
            copy_dir_recursive(&src_path, &dst_path)?;
        } else {
            fs::copy(&src_path, &dst_path)
                .map_err(|e| format!("cannot copy '{}': {}", src_path.display(), e))?;
        }
    }
    Ok(())
}

/// Write a `.kain-sysroot.json` metadata file in the installed sysroot.
fn write_sysroot_metadata(
    metadata_path: &Path,
    sysroot_name: &str,
    triple: &kain_target::TargetTriple,
    source_url: Option<&str>,
) -> Result<(), String> {
    #[derive(Serialize)]
    struct SysrootMetadata<'a> {
        name: &'a str,
        target_triple: &'a str,
        version: &'a str,
        installed_at: &'a str,
        source_url: Option<&'a str>,
    }

    let meta = SysrootMetadata {
        name: sysroot_name,
        target_triple: &triple.raw(),
        version: env!("CARGO_PKG_VERSION"),
        installed_at: &chrono::Utc::now().to_rfc3339(),
        source_url,
    };

    let json = serde_json::to_string_pretty(&meta)
        .map_err(|e| format!("cannot serialize metadata: {}", e))?;
    fs::write(metadata_path, json)
        .map_err(|e| format!("cannot write metadata: {}", e))?;
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::env;
    use tempfile::TempDir;

    #[test]
    fn publish_install_and_add_round_trip_local_package() {
        let tmp = TempDir::new().unwrap();
        let home = tmp.path().join("home");
        let package_root = tmp.path().join("kaintana");
        let project_root = tmp.path().join("app");

        fs::create_dir_all(package_root.join("src")).unwrap();
        write_text_file(
            package_root.join("KAIN.toml"),
            r#"
[package]
name = "kaintana"
version = "0.3.0"

[blade]
kind = "kain_library"
module_roots = ["src"]
"#
            .to_string(),
        )
        .unwrap();
        write_text_file(
            package_root.join("src").join("kaintana.kn"),
            "pub fn ready() -> Int:\n    return 1\n".to_string(),
        )
        .unwrap();

        fs::create_dir_all(project_root.join("src")).unwrap();
        write_text_file(
            project_root.join("src").join("main.kn"),
            "use kaintana\n\nfn main() -> Int:\n    return ready()\n".to_string(),
        )
        .unwrap();

        let previous = env::var_os("KAIN_HOME");
        env::set_var("KAIN_HOME", &home);

        let published = publish(&package_root, None, None, None, false, false, false).unwrap();
        assert!(published.source_capsule.exists());

        let installed = install(published.source_capsule.to_str().unwrap(), None).unwrap();
        assert!(installed.workspace_root.exists());

        let added = add("kaintana", None, Some(&project_root)).unwrap();
        assert!(added.manifest_path.exists());
        assert!(added.lockfile_path.exists());

        match previous {
            Some(value) => env::set_var("KAIN_HOME", value),
            None => env::remove_var("KAIN_HOME"),
        }

        let manifest = read_text_file(&added.manifest_path).unwrap();
        let lockfile = read_text_file(&added.lockfile_path).unwrap();
        assert!(manifest.contains("dependencies"));
        assert!(manifest.contains("kaintana"));
        assert!(lockfile.contains("kaintana"));
        assert!(lockfile.contains("0.3.0"));
    }
}
