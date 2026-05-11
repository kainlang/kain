use serde::{Deserialize, Serialize};
use std::collections::{BTreeMap, BTreeSet};
use std::fs;
use std::path::{Path, PathBuf};

pub const KAIN_MANIFEST_NAMES: &[&str] = &["KAIN.toml", "kain.toml"];
pub const CARGO_MANIFEST_NAME: &str = "Cargo.toml";
pub const FABRIC_MANIFEST_NAME: &str = "KAIN.fabric.toml";
pub const DEFAULT_BLADE_PATTERNS: &[&str] = &["blades/*", "apps/*", "crates/*"];

pub type BladeResult<T> = Result<T, BladeError>;

#[derive(Debug, thiserror::Error)]
pub enum BladeError {
    #[error("I/O error: {0}")]
    Io(#[from] std::io::Error),
    #[error("TOML error: {0}")]
    Toml(#[from] toml::de::Error),
    #[error("{0}")]
    Config(String),
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
#[serde(default)]
pub struct KainManifest {
    pub package: KainPackageSection,
    pub workspace: KainWorkspaceSection,
    pub build: KainBuildSection,
    pub blade: BladeSection,
    pub manifests: BTreeMap<String, PathBuf>,
    #[serde(rename = "c_ffi")]
    pub c_ffi: CffiSection,
    #[serde(rename = "rust_ffi")]
    pub rust_ffi: RustFfiSection,
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
#[serde(default)]
pub struct KainPackageSection {
    pub name: Option<String>,
    pub version: Option<String>,
    pub description: Option<String>,
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
#[serde(default)]
pub struct KainWorkspaceSection {
    pub blades: Vec<PathBuf>,
    pub blade_roots: Vec<PathBuf>,
    pub members: Vec<PathBuf>,
    pub search_roots: Vec<PathBuf>,
    pub stdlib_root: Option<PathBuf>,
    pub manifest_root: Option<PathBuf>,
    pub generated_root: Option<PathBuf>,
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
#[serde(default)]
pub struct KainBuildSection {
    pub entry: Option<PathBuf>,
    pub entry_module: Option<String>,
    pub source_root: Option<PathBuf>,
    pub source_order: Vec<PathBuf>,
    pub module_roots: Vec<PathBuf>,
    pub module_search_paths: Vec<PathBuf>,
    pub targets: Vec<String>,
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
#[serde(default)]
pub struct BladeSection {
    pub name: Option<String>,
    pub version: Option<String>,
    pub kind: Option<String>,
    pub entry: Option<PathBuf>,
    pub source_roots: Vec<PathBuf>,
    pub module_roots: Vec<PathBuf>,
    pub build_targets: Vec<String>,
    pub dependencies: Vec<BladeDependency>,
    pub cargo_manifest: Option<PathBuf>,
    pub fabric_manifest: Option<PathBuf>,
    pub runtime_contract: Option<PathBuf>,
    pub realtime_bundle: Option<PathBuf>,
    pub artifacts: BTreeMap<String, PathBuf>,
    pub rust: BladeRustSection,
    #[serde(rename = "c_ffi")]
    pub c_ffi: CffiSection,
    pub gpu: BladeGpuSection,
    pub fabric: BladeFabricSection,
}

#[derive(Debug, Clone, Default, Serialize, Deserialize, PartialEq, Eq)]
#[serde(default)]
pub struct BladeDependency {
    pub name: String,
    pub version: Option<String>,
    pub kind: Option<String>,
    pub optional: bool,
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
#[serde(default)]
pub struct BladeRustSection {
    pub cargo_manifest: Option<PathBuf>,
    pub crate_name: Option<String>,
    pub features: Vec<String>,
    pub all_features: bool,
    pub no_default_features: bool,
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
#[serde(default)]
pub struct BladeGpuSection {
    pub shader_sources: Vec<PathBuf>,
    pub shader_roots: Vec<PathBuf>,
    pub compute_keys: Vec<String>,
    pub spirv_outputs: Vec<PathBuf>,
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
#[serde(default)]
pub struct BladeFabricSection {
    pub manifest: Option<PathBuf>,
    pub entry: Option<PathBuf>,
    pub compute_key: Option<String>,
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
#[serde(default)]
pub struct RustFfiSection {
    pub manifest_path: Option<PathBuf>,
    pub path_crates: Vec<RustFfiPathCrateSection>,
    pub registry_crates: Vec<RustFfiRegistryCrateSection>,
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
#[serde(default)]
pub struct RustFfiPathCrateSection {
    pub name: String,
    pub path: PathBuf,
    pub package: Option<String>,
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
#[serde(default)]
pub struct RustFfiRegistryCrateSection {
    pub name: String,
    pub version: String,
    pub package: Option<String>,
    pub features: Vec<String>,
    pub default_features: Option<bool>,
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
#[serde(default)]
pub struct CffiSection {
    pub include_paths: Vec<PathBuf>,
    pub defines: Vec<String>,
    pub cpp_options: Vec<String>,
    pub cpp_command: Option<String>,
    pub libraries: Vec<CffiLibrarySection>,
}

#[derive(Debug, Clone, Default, Serialize, Deserialize, PartialEq, Eq)]
#[serde(default)]
pub struct CffiLibrarySection {
    pub name: String,
    pub header: PathBuf,
    pub shared_lib: Option<PathBuf>,
    pub symbols: BTreeMap<String, String>,
    pub include_paths: Vec<PathBuf>,
    pub defines: Vec<String>,
    pub cpp_options: Vec<String>,
    pub cpp_command: Option<String>,
}

#[derive(Debug, Clone, Default, Deserialize)]
#[serde(default)]
struct CargoManifest {
    package: Option<CargoPackageSection>,
}

#[derive(Debug, Clone, Default, Deserialize)]
#[serde(default)]
struct CargoPackageSection {
    name: Option<String>,
    version: Option<toml::Value>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct ResolvedCffiLibrary {
    pub name: String,
    pub header: PathBuf,
    pub shared_lib: Option<PathBuf>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct ResolvedBlade {
    pub name: String,
    pub version: Option<String>,
    pub kind: String,
    pub root: PathBuf,
    pub manifest_path: Option<PathBuf>,
    pub kain_manifest: Option<PathBuf>,
    pub cargo_manifest: Option<PathBuf>,
    pub rust_crate_name: Option<String>,
    pub fabric_manifest: Option<PathBuf>,
    pub entry: Option<PathBuf>,
    pub source_roots: Vec<PathBuf>,
    pub module_roots: Vec<PathBuf>,
    pub build_targets: Vec<String>,
    pub dependencies: Vec<BladeDependency>,
    pub artifacts: BTreeMap<String, PathBuf>,
    pub c_ffi_libraries: Vec<ResolvedCffiLibrary>,
    pub gpu_shader_sources: Vec<PathBuf>,
    pub gpu_shader_roots: Vec<PathBuf>,
    pub compute_keys: Vec<String>,
    pub discovery_source: String,
}

impl ResolvedBlade {
    pub fn is_rust_crate(&self) -> bool {
        self.cargo_manifest.is_some()
    }

    pub fn has_kain_source(&self) -> bool {
        !self.module_roots.is_empty() || self.entry.is_some() || self.kain_manifest.is_some()
    }

    pub fn has_c_ffi_library(&self, import_name: &str) -> bool {
        self.c_ffi_libraries
            .iter()
            .any(|library| names_match(&library.name, import_name))
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct BladeDiagnostic {
    pub severity: String,
    pub message: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct BladeDependencyEdge {
    pub from: String,
    pub to: String,
    pub optional: bool,
    pub kind: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct BladeWorkspace {
    pub input_path: PathBuf,
    pub root: PathBuf,
    pub manifest_path: Option<PathBuf>,
    pub blades: Vec<ResolvedBlade>,
    pub diagnostics: Vec<BladeDiagnostic>,
}

impl BladeWorkspace {
    pub fn dependency_edges(&self) -> Vec<BladeDependencyEdge> {
        let mut edges = Vec::new();
        for blade in &self.blades {
            for dependency in &blade.dependencies {
                edges.push(BladeDependencyEdge {
                    from: blade.name.clone(),
                    to: dependency.name.clone(),
                    optional: dependency.optional,
                    kind: dependency.kind.clone(),
                });
            }
        }
        edges
    }

    pub fn find_blade(&self, blade_name: &str) -> Option<&ResolvedBlade> {
        self.blades
            .iter()
            .find(|blade| names_match(&blade.name, blade_name))
    }
}

pub fn discover_workspace(start: impl AsRef<Path>) -> BladeResult<BladeWorkspace> {
    let input_path = canonicalize_lossy(start.as_ref());
    let root = discover_workspace_root(start.as_ref())?;
    let manifest_path = find_kain_manifest_in(&root);
    let manifest = manifest_path
        .as_ref()
        .map(|path| load_kain_manifest(path.as_path()))
        .transpose()?;
    let mut candidate_dirs = BTreeSet::<PathBuf>::new();

    if manifest_path.is_some() {
        candidate_dirs.insert(root.clone());
    }

    let patterns = workspace_blade_patterns(manifest.as_ref());
    for pattern in patterns {
        for candidate in expand_blade_pattern(&root, &pattern)? {
            candidate_dirs.insert(canonicalize_lossy(&candidate));
        }
    }

    let mut blades = Vec::new();
    for candidate in candidate_dirs {
        if let Some(blade) = resolve_blade_directory(&candidate)? {
            blades.push(blade);
        }
    }
    blades.sort_by(|left, right| left.name.cmp(&right.name).then(left.root.cmp(&right.root)));

    let diagnostics = duplicate_name_diagnostics(&blades);

    Ok(BladeWorkspace {
        input_path,
        root,
        manifest_path,
        blades,
        diagnostics,
    })
}

pub fn resolve_blade(start: impl AsRef<Path>, blade_name: &str) -> BladeResult<ResolvedBlade> {
    let workspace = discover_workspace(start)?;
    workspace.find_blade(blade_name).cloned().ok_or_else(|| {
        let available = workspace
            .blades
            .iter()
            .map(|blade| blade.name.as_str())
            .collect::<Vec<_>>()
            .join(", ");
        BladeError::Config(format!(
            "Could not resolve blade '{blade_name}'. Available blades: {available}"
        ))
    })
}

pub fn resolve_rust_crate_blade(
    start: impl AsRef<Path>,
    crate_name: &str,
) -> BladeResult<Option<ResolvedBlade>> {
    Ok(discover_workspace(start)?.blades.into_iter().find(|blade| {
        blade.is_rust_crate()
            && (names_match(&blade.name, crate_name)
                || blade
                    .rust_crate_name
                    .as_deref()
                    .is_some_and(|name| names_match(name, crate_name)))
    }))
}

pub fn resolve_c_ffi_library_blade(
    start: impl AsRef<Path>,
    import_name: &str,
) -> BladeResult<Option<(ResolvedBlade, ResolvedCffiLibrary)>> {
    for blade in discover_workspace(start)?.blades {
        if let Some(library) = blade
            .c_ffi_libraries
            .iter()
            .find(|library| names_match(&library.name, import_name))
            .cloned()
        {
            return Ok(Some((blade, library)));
        }
    }
    Ok(None)
}

pub fn discover_blade_module_roots_from(start: impl AsRef<Path>) -> BladeResult<Vec<PathBuf>> {
    let mut roots = BTreeSet::new();
    for blade in discover_workspace(start)?.blades {
        for root in blade.module_roots {
            if root.exists() {
                roots.insert(root);
            }
        }
    }
    Ok(roots.into_iter().collect())
}

pub fn discover_workspace_root(start: impl AsRef<Path>) -> BladeResult<PathBuf> {
    let mut current = existing_directory_anchor(start.as_ref())?;
    loop {
        if is_workspace_marker_dir(&current) {
            return Ok(canonicalize_lossy(&current));
        }
        let Some(parent) = current.parent() else {
            return Ok(canonicalize_lossy(&existing_directory_anchor(
                start.as_ref(),
            )?));
        };
        current = parent.to_path_buf();
    }
}

pub fn load_kain_manifest(path: &Path) -> BladeResult<KainManifest> {
    let source = fs::read_to_string(path)?;
    Ok(toml::from_str(&source)?)
}

fn resolve_blade_directory(candidate: &Path) -> BladeResult<Option<ResolvedBlade>> {
    if let Some(manifest_path) = find_kain_manifest_in(candidate) {
        return Ok(Some(resolve_kain_blade(candidate, &manifest_path)?));
    }

    let cargo_manifest = candidate.join(CARGO_MANIFEST_NAME);
    if cargo_manifest.exists() {
        return Ok(Some(resolve_synthetic_cargo_blade(
            candidate,
            &cargo_manifest,
        )?));
    }

    Ok(None)
}

fn resolve_kain_blade(root: &Path, manifest_path: &Path) -> BladeResult<ResolvedBlade> {
    let manifest = load_kain_manifest(manifest_path)?;
    let name = manifest
        .blade
        .name
        .clone()
        .or_else(|| manifest.package.name.clone())
        .unwrap_or_else(|| fallback_folder_name(root));
    let version = manifest
        .blade
        .version
        .clone()
        .or(manifest.package.version.clone());
    let cargo_manifest = first_existing_or_declared_path(
        root,
        [
            manifest.blade.cargo_manifest.as_deref(),
            manifest.blade.rust.cargo_manifest.as_deref(),
            manifest.rust_ffi.manifest_path.as_deref(),
        ],
    )
    .or_else(|| existing_conventional_path(root, CARGO_MANIFEST_NAME));
    let fabric_manifest = first_existing_or_declared_path(
        root,
        [
            manifest.blade.fabric_manifest.as_deref(),
            manifest.blade.fabric.manifest.as_deref(),
        ],
    )
    .or_else(|| existing_conventional_path(root, FABRIC_MANIFEST_NAME));
    let entry = first_existing_or_declared_path(
        root,
        [
            manifest.blade.entry.as_deref(),
            manifest.blade.fabric.entry.as_deref(),
            manifest.build.entry.as_deref(),
        ],
    );
    let mut source_roots = Vec::new();
    push_resolved_paths(root, &mut source_roots, &manifest.blade.source_roots);
    push_resolved_paths(root, &mut source_roots, &manifest.blade.module_roots);
    push_optional_resolved_path(root, &mut source_roots, manifest.build.source_root.as_ref());
    push_resolved_paths(root, &mut source_roots, &manifest.build.module_roots);
    push_resolved_paths(root, &mut source_roots, &manifest.build.module_search_paths);
    push_resolved_paths(root, &mut source_roots, &manifest.workspace.search_roots);
    if let Some(entry) = &entry {
        if let Some(parent) = entry.parent() {
            push_unique_path(&mut source_roots, parent.to_path_buf());
        }
    }
    for conventional in ["src", "src-kain", "src/core"] {
        let candidate = root.join(conventional);
        if candidate.exists() {
            push_unique_path(&mut source_roots, canonicalize_lossy(&candidate));
        }
    }

    let mut artifacts = BTreeMap::new();
    for (key, path) in manifest.blade.artifacts {
        artifacts.insert(key, resolve_path(root, &path));
    }
    push_named_artifact(
        root,
        &mut artifacts,
        "runtime_contract",
        manifest.blade.runtime_contract.as_ref(),
    );
    push_named_artifact(
        root,
        &mut artifacts,
        "realtime_bundle",
        manifest.blade.realtime_bundle.as_ref(),
    );
    for (key, path) in manifest.manifests {
        artifacts.insert(format!("manifest:{key}"), resolve_path(root, &path));
    }

    let mut c_ffi_libraries = resolved_c_ffi_libraries(root, &manifest.c_ffi);
    c_ffi_libraries.extend(resolved_c_ffi_libraries(root, &manifest.blade.c_ffi));
    c_ffi_libraries.sort_by(|left, right| left.name.cmp(&right.name));
    c_ffi_libraries.dedup_by(|left, right| left.name == right.name);

    let mut gpu_shader_sources = Vec::new();
    push_resolved_paths(
        root,
        &mut gpu_shader_sources,
        &manifest.blade.gpu.shader_sources,
    );
    let mut gpu_shader_roots = Vec::new();
    push_resolved_paths(
        root,
        &mut gpu_shader_roots,
        &manifest.blade.gpu.shader_roots,
    );

    let build_targets = if manifest.blade.build_targets.is_empty() {
        manifest.build.targets.clone()
    } else {
        manifest.blade.build_targets.clone()
    };

    let kind = manifest.blade.kind.clone().unwrap_or_else(|| {
        infer_blade_kind(&entry, &cargo_manifest, &fabric_manifest, &c_ffi_libraries)
    });

    Ok(ResolvedBlade {
        name,
        version,
        kind,
        root: canonicalize_lossy(root),
        manifest_path: Some(canonicalize_lossy(manifest_path)),
        kain_manifest: Some(canonicalize_lossy(manifest_path)),
        cargo_manifest,
        rust_crate_name: manifest.blade.rust.crate_name.clone(),
        fabric_manifest,
        entry,
        source_roots: source_roots.clone(),
        module_roots: source_roots,
        build_targets,
        dependencies: manifest.blade.dependencies,
        artifacts,
        c_ffi_libraries,
        gpu_shader_sources,
        gpu_shader_roots,
        compute_keys: manifest.blade.gpu.compute_keys,
        discovery_source: "kain-manifest".to_string(),
    })
}

fn resolve_synthetic_cargo_blade(root: &Path, manifest_path: &Path) -> BladeResult<ResolvedBlade> {
    let source = fs::read_to_string(manifest_path)?;
    let manifest: CargoManifest = toml::from_str(&source)?;
    let package = manifest.package.unwrap_or_default();
    let name = package.name.unwrap_or_else(|| fallback_folder_name(root));
    let version = package.version.as_ref().and_then(cargo_version_string);
    let source_root = root.join("src");
    let source_roots = if source_root.exists() {
        vec![canonicalize_lossy(&source_root)]
    } else {
        Vec::new()
    };
    let rust_crate_name = Some(name.clone());

    Ok(ResolvedBlade {
        name,
        version,
        kind: "rust_crate".to_string(),
        root: canonicalize_lossy(root),
        manifest_path: Some(canonicalize_lossy(manifest_path)),
        kain_manifest: None,
        cargo_manifest: Some(canonicalize_lossy(manifest_path)),
        rust_crate_name,
        fabric_manifest: None,
        entry: None,
        source_roots: source_roots.clone(),
        module_roots: source_roots,
        build_targets: Vec::new(),
        dependencies: Vec::new(),
        artifacts: BTreeMap::new(),
        c_ffi_libraries: Vec::new(),
        gpu_shader_sources: Vec::new(),
        gpu_shader_roots: Vec::new(),
        compute_keys: Vec::new(),
        discovery_source: "cargo-manifest".to_string(),
    })
}

fn workspace_blade_patterns(manifest: Option<&KainManifest>) -> Vec<PathBuf> {
    let mut patterns = Vec::new();
    if let Some(manifest) = manifest {
        patterns.extend(manifest.workspace.blades.iter().cloned());
        patterns.extend(manifest.workspace.blade_roots.iter().cloned());
        patterns.extend(manifest.workspace.members.iter().cloned());
    }
    if patterns.is_empty() {
        patterns.extend(DEFAULT_BLADE_PATTERNS.iter().map(PathBuf::from));
    }
    patterns
}

fn expand_blade_pattern(root: &Path, pattern: &Path) -> BladeResult<Vec<PathBuf>> {
    let pattern = if pattern.is_absolute() {
        pattern.to_path_buf()
    } else {
        root.join(pattern)
    };
    let text = pattern.to_string_lossy().replace('\\', "/");
    let Some(star_index) = text.find('*') else {
        return Ok(if pattern.exists() {
            vec![pattern]
        } else {
            Vec::new()
        });
    };
    let prefix = text[..star_index].trim_end_matches('/');
    let suffix = text[(star_index + 1)..].trim_start_matches('/');
    let base = if prefix.is_empty() {
        PathBuf::from(".")
    } else {
        PathBuf::from(prefix)
    };
    if !base.exists() {
        return Ok(Vec::new());
    }

    let mut results = Vec::new();
    for entry in fs::read_dir(base)? {
        let entry = entry?;
        if !entry.file_type()?.is_dir() {
            continue;
        }
        let candidate = if suffix.is_empty() {
            entry.path()
        } else {
            entry.path().join(suffix)
        };
        if candidate.exists() {
            results.push(candidate);
        }
    }
    results.sort();
    Ok(results)
}

fn duplicate_name_diagnostics(blades: &[ResolvedBlade]) -> Vec<BladeDiagnostic> {
    let mut seen = BTreeMap::<String, Vec<PathBuf>>::new();
    for blade in blades {
        seen.entry(normalize_name(&blade.name))
            .or_default()
            .push(blade.root.clone());
    }
    let mut diagnostics = Vec::new();
    for (name, roots) in seen {
        if roots.len() > 1 {
            diagnostics.push(BladeDiagnostic {
                severity: "warning".to_string(),
                message: format!("Blade name '{name}' is declared by multiple roots: {roots:?}"),
            });
        }
    }
    diagnostics
}

fn resolved_c_ffi_libraries(root: &Path, section: &CffiSection) -> Vec<ResolvedCffiLibrary> {
    section
        .libraries
        .iter()
        .filter(|library| !library.name.trim().is_empty())
        .map(|library| ResolvedCffiLibrary {
            name: library.name.clone(),
            header: resolve_path(root, &library.header),
            shared_lib: library
                .shared_lib
                .as_ref()
                .map(|path| resolve_path(root, path)),
        })
        .collect()
}

fn first_existing_or_declared_path<'a, I>(root: &Path, paths: I) -> Option<PathBuf>
where
    I: IntoIterator<Item = Option<&'a Path>>,
{
    let mut first_declared = None;
    for path in paths.into_iter().flatten() {
        let resolved = resolve_path(root, path);
        if first_declared.is_none() {
            first_declared = Some(resolved.clone());
        }
        if resolved.exists() {
            return Some(canonicalize_lossy(&resolved));
        }
    }
    first_declared
}

fn existing_conventional_path(root: &Path, name: &str) -> Option<PathBuf> {
    let candidate = root.join(name);
    candidate.exists().then(|| canonicalize_lossy(&candidate))
}

fn cargo_version_string(value: &toml::Value) -> Option<String> {
    match value {
        toml::Value::String(version) => Some(version.clone()),
        _ => None,
    }
}

fn infer_blade_kind(
    entry: &Option<PathBuf>,
    cargo_manifest: &Option<PathBuf>,
    fabric_manifest: &Option<PathBuf>,
    c_ffi_libraries: &[ResolvedCffiLibrary],
) -> String {
    if cargo_manifest.is_some() && entry.is_some() {
        "mixed".to_string()
    } else if cargo_manifest.is_some() {
        "rust_crate".to_string()
    } else if !c_ffi_libraries.is_empty() {
        "c_ffi".to_string()
    } else if fabric_manifest.is_some() {
        "fabric".to_string()
    } else {
        "kain".to_string()
    }
}

fn push_named_artifact(
    root: &Path,
    artifacts: &mut BTreeMap<String, PathBuf>,
    name: &str,
    path: Option<&PathBuf>,
) {
    if let Some(path) = path {
        artifacts.insert(name.to_string(), resolve_path(root, path));
    }
}

fn push_resolved_paths(root: &Path, output: &mut Vec<PathBuf>, paths: &[PathBuf]) {
    for path in paths {
        push_unique_path(output, resolve_path(root, path));
    }
}

fn push_optional_resolved_path(root: &Path, output: &mut Vec<PathBuf>, path: Option<&PathBuf>) {
    if let Some(path) = path {
        push_unique_path(output, resolve_path(root, path));
    }
}

fn push_unique_path(paths: &mut Vec<PathBuf>, path: PathBuf) {
    let canonical = canonicalize_lossy(&path);
    if !paths.iter().any(|existing| existing == &canonical) {
        paths.push(canonical);
    }
}

fn resolve_path(root: &Path, path: &Path) -> PathBuf {
    let expanded = expand_platform_dynamic_library_tokens(path);
    if expanded.is_absolute() {
        canonicalize_lossy(&expanded)
    } else {
        canonicalize_lossy(&root.join(expanded))
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

fn find_kain_manifest_in(root: &Path) -> Option<PathBuf> {
    KAIN_MANIFEST_NAMES
        .iter()
        .map(|name| root.join(name))
        .find(|path| path.exists())
        .map(|path| canonicalize_lossy(&path))
}

fn is_workspace_marker_dir(dir: &Path) -> bool {
    KAIN_MANIFEST_NAMES
        .iter()
        .any(|name| dir.join(name).exists())
        || dir.join(CARGO_MANIFEST_NAME).exists()
        || dir.join(".git").exists()
}

fn existing_directory_anchor(path: &Path) -> BladeResult<PathBuf> {
    if path.exists() {
        if path.is_dir() {
            return Ok(canonicalize_lossy(path));
        }
        if let Some(parent) = path.parent() {
            return Ok(canonicalize_lossy(parent));
        }
    }

    if let Some(parent) = path.parent().filter(|parent| parent.exists()) {
        return Ok(canonicalize_lossy(parent));
    }

    Ok(std::env::current_dir()?)
}

fn fallback_folder_name(path: &Path) -> String {
    path.file_name()
        .and_then(|value| value.to_str())
        .filter(|value| !value.trim().is_empty())
        .unwrap_or("anonymous-blade")
        .to_string()
}

fn names_match(left: &str, right: &str) -> bool {
    left == right || normalize_name(left) == normalize_name(right)
}

fn normalize_name(value: &str) -> String {
    value.trim().replace('_', "-").to_ascii_lowercase()
}

fn canonicalize_lossy(path: &Path) -> PathBuf {
    fs::canonicalize(path).unwrap_or_else(|_| path.to_path_buf())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn discovers_kain_blade_from_default_blades_root() {
        let tmp = tempfile::tempdir().unwrap();
        fs::write(tmp.path().join("Cargo.toml"), "[workspace]\nmembers = []\n").unwrap();
        let blade_root = tmp.path().join("blades").join("fabric");
        fs::create_dir_all(blade_root.join("src")).unwrap();
        fs::write(
            blade_root.join("KAIN.toml"),
            r#"
[package]
name = "fabric-blade"
version = "0.1.0"

[build]
entry = "src/main.kn"
targets = ["run", "hybrid"]
"#,
        )
        .unwrap();
        fs::write(
            blade_root.join("src").join("main.kn"),
            "fn main() -> Int:\n    return 1\n",
        )
        .unwrap();

        let workspace = discover_workspace(tmp.path()).unwrap();
        let blade = workspace.find_blade("fabric_blade").unwrap();
        assert_eq!(blade.name, "fabric-blade");
        assert_eq!(blade.kind, "kain");
        assert_eq!(blade.build_targets, vec!["run", "hybrid"]);
        assert!(blade.entry.as_ref().unwrap().ends_with("src/main.kn"));
    }

    #[test]
    fn discovers_synthetic_rust_crate_from_crates_root() {
        let tmp = tempfile::tempdir().unwrap();
        fs::write(tmp.path().join("Cargo.toml"), "[workspace]\nmembers = []\n").unwrap();
        let crate_root = tmp.path().join("crates").join("native_math");
        fs::create_dir_all(crate_root.join("src")).unwrap();
        fs::write(
            crate_root.join("Cargo.toml"),
            "[package]\nname = \"native-math\"\nversion = \"0.2.0\"\nedition = \"2021\"\n",
        )
        .unwrap();

        let blade = resolve_rust_crate_blade(tmp.path(), "native_math")
            .unwrap()
            .unwrap();
        assert_eq!(blade.name, "native-math");
        assert_eq!(blade.kind, "rust_crate");
        assert!(blade
            .cargo_manifest
            .as_ref()
            .unwrap()
            .ends_with("Cargo.toml"));
    }

    #[test]
    fn resolves_c_ffi_library_blade_by_library_name() {
        let tmp = tempfile::tempdir().unwrap();
        fs::write(tmp.path().join("Cargo.toml"), "[workspace]\nmembers = []\n").unwrap();
        let blade_root = tmp.path().join("blades").join("native_ops");
        fs::create_dir_all(blade_root.join("native")).unwrap();
        fs::write(
            blade_root.join("native").join("ops.h"),
            "int add(int a, int b);\n",
        )
        .unwrap();
        fs::write(
            blade_root.join("KAIN.toml"),
            r#"
[package]
name = "native-ops"

[c_ffi]
[[c_ffi.libraries]]
name = "ops"
header = "native/ops.h"
"#,
        )
        .unwrap();

        let (blade, library) = resolve_c_ffi_library_blade(tmp.path(), "ops")
            .unwrap()
            .unwrap();
        assert_eq!(blade.name, "native-ops");
        assert_eq!(library.name, "ops");
        assert!(library.header.ends_with("native/ops.h"));
    }

    #[test]
    fn workspace_manifest_can_override_blade_patterns() {
        let tmp = tempfile::tempdir().unwrap();
        fs::write(
            tmp.path().join("KAIN.toml"),
            "[workspace]\nblades = [\"packages/*\"]\n",
        )
        .unwrap();
        let blade_root = tmp.path().join("packages").join("omni");
        fs::create_dir_all(blade_root.join("src")).unwrap();
        fs::write(
            blade_root.join("KAIN.toml"),
            "[package]\nname = \"omni\"\n\n[build]\nentry = \"src/main.kn\"\n",
        )
        .unwrap();

        let workspace = discover_workspace(tmp.path()).unwrap();
        assert!(workspace.find_blade("omni").is_some());
    }
}
