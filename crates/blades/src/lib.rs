use kain_fs as kfs;
use kain_fs::FsFileType;
use serde::{Deserialize, Serialize};
use std::collections::{BTreeMap, BTreeSet};
use std::path::{Path, PathBuf};

mod package_store;
pub use package_store::*;

pub const KAIN_MANIFEST_NAMES: &[&str] = &["KAIN.toml", "kain.toml"];
pub const KAIN_BUILD_SCRIPT_NAMES: &[&str] = &["build.kn", "platform.kn"];
pub const CARGO_MANIFEST_NAME: &str = "Cargo.toml";
pub const FABRIC_MANIFEST_NAME: &str = "KAIN.fabric.toml";
pub const DEFAULT_BLADE_PATTERNS: &[&str] = &["blades/*", "apps/*", "crates/*"];

pub type BladeResult<T> = Result<T, BladeError>;

#[derive(Debug, thiserror::Error)]
pub enum BladeError {
    #[error("I/O error: {0}")]
    Io(#[from] std::io::Error),
    #[error("filesystem error: {0}")]
    Fs(#[from] kain_fs::FsError),
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
    pub run: KainRunSection,
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
    pub artifact_root: Option<PathBuf>,
    pub cache_root: Option<PathBuf>,
    pub profile: Option<String>,
    pub tasks: Vec<KainBuildTaskSection>,
}

#[derive(Debug, Clone, Default, Serialize, Deserialize, PartialEq, Eq)]
#[serde(default)]
pub struct KainBuildTaskSection {
    pub id: String,
    pub kind: String,
    pub blade: Option<String>,
    pub entry: Option<PathBuf>,
    pub manifest: Option<PathBuf>,
    pub command: Option<String>,
    pub args: Vec<String>,
    pub cwd: Option<PathBuf>,
    pub target: Option<String>,
    pub profile: Option<String>,
    pub inputs: Vec<PathBuf>,
    pub outputs: Vec<PathBuf>,
    pub depends_on: Vec<String>,
    pub required_capabilities: Vec<String>,
    pub matrix_axes: Vec<String>,
    pub telemetry: Vec<String>,
    pub certifies: Vec<String>,
    pub env: BTreeMap<String, String>,
    pub options: BTreeMap<String, String>,
    pub tags: Vec<String>,
    pub notes: Vec<String>,
    pub authors: Vec<String>,
    pub meta: BTreeMap<String, String>,
}

#[derive(Debug, Clone, Default, Serialize, Deserialize, PartialEq, Eq)]
#[serde(default)]
pub struct KainRunSection {
    pub entry: Option<PathBuf>,
    pub blade: Option<String>,
    pub target: Option<String>,
    pub args: Vec<String>,
    pub env: BTreeMap<String, String>,
    pub cwd: Option<PathBuf>,
    pub watch: Vec<PathBuf>,
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
    pub link_libs: Vec<String>,
    pub cpp_options: Vec<String>,
    pub cpp_command: Option<String>,
    pub libraries: Vec<CffiLibrarySection>,
}

#[derive(Debug, Clone, Default, Serialize, Deserialize, PartialEq, Eq)]
#[serde(default)]
pub struct CffiLibrarySection {
    pub name: String,
    pub header: PathBuf,
    pub sources: Vec<PathBuf>,
    pub shared_lib: Option<PathBuf>,
    pub symbols: BTreeMap<String, String>,
    pub include_paths: Vec<PathBuf>,
    pub defines: Vec<String>,
    pub link_libs: Vec<String>,
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
    pub sources: Vec<PathBuf>,
    pub shared_lib: Option<PathBuf>,
    pub include_paths: Vec<PathBuf>,
    pub defines: Vec<String>,
    pub link_libs: Vec<String>,
    pub cpp_options: Vec<String>,
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

    pub fn transitive_c_ffi_libraries_for(&self, blade_name: &str) -> Vec<ResolvedCffiLibrary> {
        let mut output = Vec::new();
        let mut visited = BTreeSet::new();
        self.collect_transitive_c_ffi_libraries(blade_name, &mut visited, &mut output);
        output.sort_by(|left, right| {
            left.name
                .cmp(&right.name)
                .then(left.header.cmp(&right.header))
        });
        output.dedup_by(|left, right| left.name == right.name && left.header == right.header);
        output
    }

    fn collect_transitive_c_ffi_libraries(
        &self,
        blade_name: &str,
        visited: &mut BTreeSet<String>,
        output: &mut Vec<ResolvedCffiLibrary>,
    ) {
        let Some(blade) = self.find_blade(blade_name) else {
            return;
        };
        if !visited.insert(normalize_name(&blade.name)) {
            return;
        }
        output.extend(blade.c_ffi_libraries.iter().cloned());
        for dependency in &blade.dependencies {
            self.collect_transitive_c_ffi_libraries(&dependency.name, visited, output);
        }
    }
}

pub fn discover_workspace(start: impl AsRef<Path>) -> BladeResult<BladeWorkspace> {
    let input_path = canonicalize_lossy(start.as_ref());
    let root = discover_workspace_root(start.as_ref())?;
    let manifest_path = find_kain_manifest_in(&root);
    let manifest = load_effective_kain_manifest(&root)?;
    let mut candidate_dirs = BTreeSet::<PathBuf>::new();

    if manifest_path.is_some()
        || manifest
            .as_ref()
            .is_some_and(manifest_declares_blade_surface)
    {
        candidate_dirs.insert(root.clone());
    }

    let patterns = workspace_blade_patterns(manifest.as_ref());
    for pattern in patterns {
        for candidate in expand_blade_pattern(&root, &pattern)? {
            candidate_dirs.insert(canonicalize_lossy(&candidate));
        }
    }

    for candidate in declared_installed_package_workspace_roots_for(&root)? {
        candidate_dirs.insert(candidate);
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
    let mut visited_workspaces = BTreeSet::new();
    let mut current = existing_directory_anchor(start.as_ref())?;
    loop {
        if is_workspace_marker_dir(&current) {
            let workspace = discover_workspace(&current)?;
            if visited_workspaces.insert(workspace.root.clone()) {
                push_generated_platform_module_roots(&workspace.root, &mut roots)?;
                for blade in workspace.blades {
                    for root in blade.module_roots {
                        if root.exists() {
                            roots.insert(root);
                        }
                    }
                }
            }
        }
        let Some(parent) = current.parent() else {
            break;
        };
        current = parent.to_path_buf();
    }
    for root in ambient_installed_package_module_roots()? {
        if root.exists() {
            roots.insert(root);
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
    let source = kfs::read_text(path)?;
    Ok(toml::from_str(&source)?)
}

pub fn load_effective_kain_manifest(root: &Path) -> BladeResult<Option<KainManifest>> {
    let manifest_path = find_kain_manifest_in(root);
    let manifest = manifest_path
        .as_ref()
        .map(|path| load_kain_manifest(path.as_path()))
        .transpose()?;
    let build_script = find_build_script_in(root);

    if manifest.is_none() && build_script.is_none() {
        return Ok(None);
    }

    let mut effective = manifest.unwrap_or_default();
    if let Some(build_script) = build_script {
        let source = kfs::read_text(&build_script)?;
        let overlay = extract_build_script_manifest(&source);
        effective = merge_kain_manifest(effective, overlay);
    }
    Ok(Some(effective))
}

fn resolve_blade_directory(candidate: &Path) -> BladeResult<Option<ResolvedBlade>> {
    if let Some(manifest_path) = find_kain_manifest_in(candidate) {
        if !load_effective_kain_manifest(candidate)?
            .as_ref()
            .is_some_and(manifest_declares_blade_surface)
        {
            return Ok(None);
        }
        return Ok(Some(resolve_authored_blade(
            candidate,
            Some(&manifest_path),
        )?));
    }

    if find_build_script_in(candidate).is_some() {
        if load_effective_kain_manifest(candidate)?
            .as_ref()
            .is_some_and(manifest_declares_blade_surface)
        {
            return Ok(Some(resolve_authored_blade(candidate, None)?));
        }
        return Ok(None);
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

fn resolve_authored_blade(root: &Path, manifest_path: Option<&Path>) -> BladeResult<ResolvedBlade> {
    let manifest = load_effective_kain_manifest(root)?.ok_or_else(|| {
        BladeError::Config(format!(
            "No effective Kain manifest or build script could be loaded for {}",
            root.display()
        ))
    })?;
    if !manifest_declares_blade_surface(&manifest) {
        return Err(BladeError::Config(format!(
            "Authored blade root {} does not declare blade metadata or entry points",
            root.display()
        )));
    }
    let build_script = find_build_script_in(root);
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
    let mut module_roots = source_roots.clone();
    expand_module_roots_from_kn_files(&mut module_roots)?;

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
        manifest_path: manifest_path.map(canonicalize_lossy),
        kain_manifest: manifest_path.map(canonicalize_lossy),
        cargo_manifest,
        rust_crate_name: manifest.blade.rust.crate_name.clone(),
        fabric_manifest,
        entry,
        source_roots: source_roots.clone(),
        module_roots,
        build_targets,
        dependencies: manifest.blade.dependencies,
        artifacts,
        c_ffi_libraries,
        gpu_shader_sources,
        gpu_shader_roots,
        compute_keys: manifest.blade.gpu.compute_keys,
        discovery_source: match (manifest_path.is_some(), build_script.is_some()) {
            (true, true) => "kain-manifest+build-script".to_string(),
            (true, false) => "kain-manifest".to_string(),
            (false, true) => "build-script".to_string(),
            (false, false) => "kain-manifest".to_string(),
        },
    })
}

fn resolve_synthetic_cargo_blade(root: &Path, manifest_path: &Path) -> BladeResult<ResolvedBlade> {
    let source = kfs::read_text(manifest_path)?;
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

fn manifest_declares_blade_surface(manifest: &KainManifest) -> bool {
    manifest.blade.name.is_some()
        || manifest.blade.kind.is_some()
        || manifest.blade.entry.is_some()
        || manifest.build.entry.is_some()
        || manifest.run.entry.is_some()
        || manifest.run.blade.is_some()
        || !manifest.blade.source_roots.is_empty()
        || !manifest.blade.module_roots.is_empty()
        || !manifest.blade.build_targets.is_empty()
        || !manifest.blade.dependencies.is_empty()
        || manifest.blade.cargo_manifest.is_some()
        || manifest.blade.fabric_manifest.is_some()
        || !manifest.c_ffi.libraries.is_empty()
        || !manifest.blade.c_ffi.libraries.is_empty()
        || !manifest.blade.gpu.shader_sources.is_empty()
        || !manifest.blade.gpu.shader_roots.is_empty()
        || !manifest.blade.gpu.compute_keys.is_empty()
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
    for entry in kfs::read_dir_entries(base)? {
        if entry.file_type != FsFileType::Directory {
            continue;
        }
        let candidate = if suffix.is_empty() {
            entry.path
        } else {
            entry.path.join(suffix)
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
            sources: library
                .sources
                .iter()
                .map(|path| resolve_path(root, path))
                .collect(),
            shared_lib: library
                .shared_lib
                .as_ref()
                .map(|path| resolve_path(root, path)),
            include_paths: library
                .include_paths
                .iter()
                .map(|path| resolve_path(root, path))
                .collect(),
            defines: library.defines.clone(),
            link_libs: library
                .link_libs
                .iter()
                .cloned()
                .chain(section.link_libs.iter().cloned())
                .collect(),
            cpp_options: library.cpp_options.clone(),
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

fn expand_module_roots_from_kn_files(module_roots: &mut Vec<PathBuf>) -> BladeResult<()> {
    let seeds = module_roots.clone();
    for seed in seeds {
        collect_kn_module_dirs(&seed, module_roots, 0)?;
    }
    Ok(())
}

fn push_generated_platform_module_roots(
    workspace_root: &Path,
    roots: &mut BTreeSet<PathBuf>,
) -> BladeResult<()> {
    let platform_root = workspace_root.join(".kain").join("platform");
    if !platform_root.exists() {
        return Ok(());
    }
    let mut module_roots = Vec::new();
    collect_kn_module_dirs(&platform_root, &mut module_roots, 0)?;
    roots.extend(module_roots);
    Ok(())
}

fn collect_kn_module_dirs(
    root: &Path,
    module_roots: &mut Vec<PathBuf>,
    depth: usize,
) -> BladeResult<()> {
    const MAX_MODULE_DISCOVERY_DEPTH: usize = 32;
    if depth > MAX_MODULE_DISCOVERY_DEPTH || !root.exists() || !root.is_dir() {
        return Ok(());
    }

    let entries = kfs::read_dir_entries(root)?;
    let has_kain_file = entries.iter().any(|entry| {
        entry.file_type == FsFileType::File
            && entry
                .path
                .extension()
                .and_then(|ext| ext.to_str())
                .is_some_and(|ext| ext == "kn")
    });
    if has_kain_file {
        push_unique_path(module_roots, root.to_path_buf());
    }

    for entry in entries {
        if entry.file_type != FsFileType::Directory {
            continue;
        }
        if entry
            .path
            .file_name()
            .and_then(|name| name.to_str())
            .is_some_and(skip_module_discovery_dir)
        {
            continue;
        }
        collect_kn_module_dirs(&entry.path, module_roots, depth + 1)?;
    }
    Ok(())
}

fn skip_module_discovery_dir(name: &str) -> bool {
    matches!(
        name,
        ".git" | ".kain" | "target" | "node_modules" | "__pycache__" | "out" | "dist"
    )
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

fn find_kain_manifest_in(root: &Path) -> Option<PathBuf> {
    KAIN_MANIFEST_NAMES
        .iter()
        .map(|name| root.join(name))
        .find(|path| path.exists())
        .map(|path| canonicalize_lossy(&path))
}

pub fn find_build_script_in(root: &Path) -> Option<PathBuf> {
    KAIN_BUILD_SCRIPT_NAMES
        .iter()
        .map(|name| root.join(name))
        .find(|path| path.exists())
        .map(|path| canonicalize_lossy(&path))
}

fn is_workspace_marker_dir(dir: &Path) -> bool {
    KAIN_MANIFEST_NAMES
        .iter()
        .any(|name| dir.join(name).exists())
        || KAIN_BUILD_SCRIPT_NAMES
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
    kfs::canonicalize_path(path)
        .map(PathBuf::from)
        .unwrap_or_else(|_| path.to_path_buf())
}

fn extract_build_script_manifest(source: &str) -> KainManifest {
    let mut manifest = KainManifest::default();
    apply_package_manifest_from_build_script(source, &mut manifest);
    apply_blade_manifest_from_build_script(source, &mut manifest);
    apply_project_manifest_from_build_script(source, &mut manifest);
    apply_blade_dependencies_from_build_script(source, &mut manifest);
    apply_build_defaults_from_build_script(source, &mut manifest);
    apply_run_defaults_from_build_script(source, &mut manifest);
    apply_workspace_defaults_from_build_script(source, &mut manifest);
    manifest.build.tasks = extract_build_script_explicit_tasks(source);
    manifest
}

fn apply_package_manifest_from_build_script(source: &str, manifest: &mut KainManifest) {
    for (args, methods) in scan_string_call_chains(source, "package") {
        if let Some(name) = args.first() {
            manifest.package.name = Some(name.clone());
        }
        for (method, values, _) in methods {
            let Some(value) = values.first() else {
                continue;
            };
            match method.as_str() {
                "version" => manifest.package.version = Some(value.clone()),
                "description" => manifest.package.description = Some(value.clone()),
                _ => {}
            }
        }
    }
}

fn apply_blade_manifest_from_build_script(source: &str, manifest: &mut KainManifest) {
    for (args, methods) in scan_string_call_chains(source, "blade") {
        if let Some(name) = args.first() {
            manifest.blade.name = Some(name.clone());
        }
        for (method, values, _) in methods {
            match method.as_str() {
                "version" => assign_first_string(&values, &mut manifest.blade.version),
                "kind" => assign_first_string(&values, &mut manifest.blade.kind),
                "entry" => assign_first_path(&values, &mut manifest.blade.entry),
                "source_root" | "source_roots" => {
                    push_unique_paths_from_strings(&mut manifest.blade.source_roots, &values);
                }
                "module_root" | "module_roots" => {
                    push_unique_paths_from_strings(&mut manifest.blade.module_roots, &values);
                }
                "build_target" | "build_targets" | "target" | "targets" => {
                    push_unique_strings(&mut manifest.blade.build_targets, &values);
                }
                "dependency" | "depends_on_blade" => {
                    for value in values {
                        push_unique_dependency(
                            &mut manifest.blade.dependencies,
                            BladeDependency {
                                name: value,
                                version: None,
                                kind: None,
                                optional: false,
                            },
                        );
                    }
                }
                "cargo_manifest" => assign_first_path(&values, &mut manifest.blade.cargo_manifest),
                "fabric_manifest" => {
                    assign_first_path(&values, &mut manifest.blade.fabric_manifest)
                }
                "runtime_contract" => {
                    assign_first_path(&values, &mut manifest.blade.runtime_contract)
                }
                "realtime_bundle" => {
                    assign_first_path(&values, &mut manifest.blade.realtime_bundle)
                }
                "gpu_shader_source" | "shader_source" => {
                    push_unique_paths_from_strings(&mut manifest.blade.gpu.shader_sources, &values);
                }
                "gpu_shader_root" | "shader_root" => {
                    push_unique_paths_from_strings(&mut manifest.blade.gpu.shader_roots, &values);
                }
                "compute_key" | "gpu_compute_key" => {
                    push_unique_strings(&mut manifest.blade.gpu.compute_keys, &values);
                }
                _ => {}
            }
        }
    }
}

fn apply_project_manifest_from_build_script(source: &str, manifest: &mut KainManifest) {
    // The new-style project() is a combined package+blade declaration. The string scanner
    // mirrors merge_project_into_manifest() in the Rust evaluator so that blades using the
    // new DAG build.kn API are still discoverable by the legacy scanner.
    for (args, methods) in scan_string_call_chains(source, "project") {
        if let Some(name) = args.first() {
            manifest.package.name = Some(name.clone());
            manifest.blade.name = Some(name.clone());
        }
        for (method, values, _) in methods {
            match method.as_str() {
                "version" => {
                    assign_first_string(&values, &mut manifest.package.version);
                    assign_first_string(&values, &mut manifest.blade.version);
                }
                "description" => {
                    assign_first_string(&values, &mut manifest.package.description);
                }
                "kind" => {
                    assign_first_string(&values, &mut manifest.blade.kind);
                }
                "entry" => {
                    assign_first_path(&values, &mut manifest.blade.entry);
                    assign_first_path(&values, &mut manifest.build.entry);
                }
                "source_root" | "source_roots" => {
                    push_unique_paths_from_strings(&mut manifest.blade.source_roots, &values);
                }
                "module_root" | "module_roots" => {
                    push_unique_paths_from_strings(&mut manifest.blade.module_roots, &values);
                    push_unique_paths_from_strings(&mut manifest.build.module_roots, &values);
                }
                "target" | "targets" | "build_target" | "build_targets" => {
                    push_unique_strings(&mut manifest.blade.build_targets, &values);
                    push_unique_strings(&mut manifest.build.targets, &values);
                }
                "artifact_root" => {
                    assign_first_path(&values, &mut manifest.build.artifact_root);
                }
                "cache_root" => {
                    assign_first_path(&values, &mut manifest.build.cache_root);
                }
                "profile" => {
                    assign_first_string(&values, &mut manifest.build.profile);
                }
                _ => {}
            }
        }
    }
}

fn apply_blade_dependencies_from_build_script(source: &str, manifest: &mut KainManifest) {
    for (args, methods) in scan_string_call_chains(source, "blade_dependency") {
        let Some(name) = args.first() else {
            continue;
        };
        let mut dependency = BladeDependency {
            name: name.clone(),
            version: None,
            kind: None,
            optional: false,
        };
        for (method, values, _) in methods {
            let Some(value) = values.first() else {
                continue;
            };
            match method.as_str() {
                "version" => dependency.version = Some(value.clone()),
                "kind" => dependency.kind = Some(value.clone()),
                "optional" => dependency.optional = parse_bool_string(value),
                _ => {}
            }
        }
        push_unique_dependency(&mut manifest.blade.dependencies, dependency);
    }
}

fn apply_build_defaults_from_build_script(source: &str, manifest: &mut KainManifest) {
    for (_, methods) in scan_string_call_chains(source, "build_defaults") {
        for (method, values, _) in methods {
            match method.as_str() {
                "entry" => assign_first_path(&values, &mut manifest.build.entry),
                "entry_module" => assign_first_string(&values, &mut manifest.build.entry_module),
                "source_root" => assign_first_path(&values, &mut manifest.build.source_root),
                "source_order" => {
                    push_unique_paths_from_strings(&mut manifest.build.source_order, &values);
                }
                "module_root" | "module_roots" => {
                    push_unique_paths_from_strings(&mut manifest.build.module_roots, &values);
                }
                "module_search_path" | "module_search_paths" => {
                    push_unique_paths_from_strings(
                        &mut manifest.build.module_search_paths,
                        &values,
                    );
                }
                "target" | "targets" => push_unique_strings(&mut manifest.build.targets, &values),
                "artifact_root" => assign_first_path(&values, &mut manifest.build.artifact_root),
                "cache_root" => assign_first_path(&values, &mut manifest.build.cache_root),
                "profile" => assign_first_string(&values, &mut manifest.build.profile),
                _ => {}
            }
        }
    }
}

fn apply_run_defaults_from_build_script(source: &str, manifest: &mut KainManifest) {
    for (_, methods) in scan_string_call_chains(source, "run_defaults") {
        for (method, values, _) in methods {
            match method.as_str() {
                "entry" => assign_first_path(&values, &mut manifest.run.entry),
                "blade" => assign_first_string(&values, &mut manifest.run.blade),
                "target" => assign_first_string(&values, &mut manifest.run.target),
                "arg" | "args" => manifest.run.args.extend(values),
                "cwd" => assign_first_path(&values, &mut manifest.run.cwd),
                "watch" | "watch_path" => {
                    push_unique_paths_from_strings(&mut manifest.run.watch, &values);
                }
                "env" if values.len() >= 2 => {
                    manifest
                        .run
                        .env
                        .insert(values[0].clone(), values[1].clone());
                }
                _ => {}
            }
        }
    }
}

fn apply_workspace_defaults_from_build_script(source: &str, manifest: &mut KainManifest) {
    for (_, methods) in scan_string_call_chains(source, "workspace_defaults") {
        for (method, values, _) in methods {
            match method.as_str() {
                "blade_pattern" | "blades" => {
                    push_unique_paths_from_strings(&mut manifest.workspace.blades, &values);
                }
                "blade_root" | "blade_roots" => {
                    push_unique_paths_from_strings(&mut manifest.workspace.blade_roots, &values);
                }
                "member" | "members" => {
                    push_unique_paths_from_strings(&mut manifest.workspace.members, &values);
                }
                "search_root" | "search_roots" => {
                    push_unique_paths_from_strings(&mut manifest.workspace.search_roots, &values);
                }
                "stdlib_root" => assign_first_path(&values, &mut manifest.workspace.stdlib_root),
                "manifest_root" => {
                    assign_first_path(&values, &mut manifest.workspace.manifest_root)
                }
                "generated_root" => {
                    assign_first_path(&values, &mut manifest.workspace.generated_root)
                }
                _ => {}
            }
        }
    }
}

const BUILD_TASK_CONSTRUCTORS: &[(&str, Option<&str>)] = &[
    ("build_task", None),
    ("build_check", Some("check")),
    ("check_task", Some("check")),
    ("exec_task", Some("exec")),
    ("command_task", Some("exec")),
    ("amalgamate_capsule", Some("amalgamate")),
    ("capsule_task", Some("amalgamate")),
    ("native_executable", Some("native-executable")),
    ("root_executable", Some("native-executable")),
    ("build_native_executable", Some("native-executable")),
    ("test_task", Some("test")),
    ("test_suite", Some("test")),
    ("proof_task", Some("proof")),
    ("proof_obligation", Some("proof")),
    ("z3_proof", Some("proof")),
    ("bench_task", Some("benchmark")),
    ("bench_case", Some("benchmark")),
    ("benchmark_task", Some("benchmark")),
    ("attrition_task", Some("attrition")),
    ("attrition_case", Some("attrition")),
    ("certify_task", Some("certify")),
    ("certify_gate", Some("certify")),
    ("release_gate", Some("certify")),
];

fn extract_build_script_explicit_tasks(source: &str) -> Vec<KainBuildTaskSection> {
    let mut tasks = Vec::new();
    for (constructor, default_kind) in BUILD_TASK_CONSTRUCTORS {
        for (args, methods) in scan_string_call_chains(source, constructor) {
            let Some(id) = args.first() else {
                continue;
            };
            let mut task = KainBuildTaskSection {
                id: id.clone(),
                kind: default_kind.unwrap_or_default().to_string(),
                ..KainBuildTaskSection::default()
            };
            for (method, values, _) in methods {
                match method.as_str() {
                    "kind" => {
                        if let Some(value) = values.first() {
                            task.kind = value.clone();
                        }
                    }
                    "blade" => assign_first_string(&values, &mut task.blade),
                    "entry" | "source" | "path" => assign_first_path(&values, &mut task.entry),
                    "manifest" => assign_first_path(&values, &mut task.manifest),
                    "command" => assign_first_string(&values, &mut task.command),
                    "arg" | "args" => task.args.extend(values),
                    "cwd" => assign_first_path(&values, &mut task.cwd),
                    "target" => assign_first_string(&values, &mut task.target),
                    "profile" => assign_first_string(&values, &mut task.profile),
                    "input" | "inputs" => push_unique_paths_from_strings(&mut task.inputs, &values),
                    "output" | "outputs" | "root_output" | "blade_output" | "artifact" => {
                        push_unique_paths_from_strings(&mut task.outputs, &values)
                    }
                    "depends_on" | "depends" | "dependency" | "requires" | "requires_task" => {
                        push_unique_strings(&mut task.depends_on, &values)
                    }
                    "requires_capability" | "when_capability" | "capability" => {
                        push_unique_strings(&mut task.required_capabilities, &values)
                    }
                    "axis" | "matrix_axis" | "matrix_value" | "matrix" => push_unique_strings(
                        &mut task.matrix_axes,
                        &canonical_matrix_axis_values(values),
                    ),
                    "telemetry" | "telemetry_channel" => {
                        push_unique_strings(&mut task.telemetry, &values)
                    }
                    "certifies" | "certificate" => {
                        push_unique_strings(&mut task.certifies, &values)
                    }
                    "env" => insert_pair(&values, &mut task.env),
                    "meta" => insert_pair(&values, &mut task.meta),
                    "option" => insert_pair(&values, &mut task.options),
                    "tag" => push_unique_strings(&mut task.tags, &values),
                    "note" => push_unique_strings(&mut task.notes, &values),
                    "author" => push_unique_strings(&mut task.authors, &values),
                    "name" | "version" | "storage" | "contents" | "capsule_set" | "header"
                    | "compression" | "preview_symbols" | "api_index" | "module_index"
                    | "timeout_ms" | "stdout" | "stderr" => {
                        if let Some(value) = values.first() {
                            task.options.insert(method.clone(), value.clone());
                        }
                    }
                    "archive" => {
                        let enabled = values
                            .first()
                            .map_or(true, |value| parse_bool_string(value));
                        task.options.insert(
                            "storage".to_string(),
                            if enabled { "archive" } else { "editable" }.to_string(),
                        );
                    }
                    "editable" => {
                        task.options
                            .insert("storage".to_string(), "editable".to_string());
                    }
                    "always_run" => {
                        task.options
                            .insert("always_run".to_string(), "true".to_string());
                    }
                    "proof_mode" | "mode" => task.args.extend(values),
                    _ => {}
                }
            }
            tasks.push(task);
        }
    }
    tasks.sort_by(|left, right| left.id.cmp(&right.id).then(left.kind.cmp(&right.kind)));
    tasks.dedup_by(|left, right| left.id == right.id && left.kind == right.kind);
    tasks
}

fn canonical_matrix_axis_values(values: Vec<String>) -> Vec<String> {
    if values.len() == 2 {
        vec![format!("{}={}", values[0], values[1])]
    } else {
        values
    }
}

fn merge_kain_manifest(mut base: KainManifest, overlay: KainManifest) -> KainManifest {
    merge_package_section(&mut base.package, overlay.package);
    merge_workspace_section(&mut base.workspace, overlay.workspace);
    merge_build_section(&mut base.build, overlay.build);
    merge_run_section(&mut base.run, overlay.run);
    merge_blade_section(&mut base.blade, overlay.blade);
    base.manifests.extend(overlay.manifests);
    base
}

fn merge_package_section(base: &mut KainPackageSection, overlay: KainPackageSection) {
    overlay_optional(&mut base.name, overlay.name);
    overlay_optional(&mut base.version, overlay.version);
    overlay_optional(&mut base.description, overlay.description);
}

fn merge_workspace_section(base: &mut KainWorkspaceSection, overlay: KainWorkspaceSection) {
    overlay_vec(&mut base.blades, overlay.blades);
    overlay_vec(&mut base.blade_roots, overlay.blade_roots);
    overlay_vec(&mut base.members, overlay.members);
    overlay_vec(&mut base.search_roots, overlay.search_roots);
    overlay_optional(&mut base.stdlib_root, overlay.stdlib_root);
    overlay_optional(&mut base.manifest_root, overlay.manifest_root);
    overlay_optional(&mut base.generated_root, overlay.generated_root);
}

fn merge_build_section(base: &mut KainBuildSection, overlay: KainBuildSection) {
    overlay_optional(&mut base.entry, overlay.entry);
    overlay_optional(&mut base.entry_module, overlay.entry_module);
    overlay_optional(&mut base.source_root, overlay.source_root);
    overlay_vec(&mut base.source_order, overlay.source_order);
    overlay_vec(&mut base.module_roots, overlay.module_roots);
    overlay_vec(&mut base.module_search_paths, overlay.module_search_paths);
    overlay_vec(&mut base.targets, overlay.targets);
    overlay_optional(&mut base.artifact_root, overlay.artifact_root);
    overlay_optional(&mut base.cache_root, overlay.cache_root);
    overlay_optional(&mut base.profile, overlay.profile);
    overlay_vec(&mut base.tasks, overlay.tasks);
}

fn merge_run_section(base: &mut KainRunSection, overlay: KainRunSection) {
    overlay_optional(&mut base.entry, overlay.entry);
    overlay_optional(&mut base.blade, overlay.blade);
    overlay_optional(&mut base.target, overlay.target);
    overlay_vec(&mut base.args, overlay.args);
    base.env.extend(overlay.env);
    overlay_optional(&mut base.cwd, overlay.cwd);
    overlay_vec(&mut base.watch, overlay.watch);
}

fn merge_blade_section(base: &mut BladeSection, overlay: BladeSection) {
    overlay_optional(&mut base.name, overlay.name);
    overlay_optional(&mut base.version, overlay.version);
    overlay_optional(&mut base.kind, overlay.kind);
    overlay_optional(&mut base.entry, overlay.entry);
    overlay_vec(&mut base.source_roots, overlay.source_roots);
    overlay_vec(&mut base.module_roots, overlay.module_roots);
    overlay_vec(&mut base.build_targets, overlay.build_targets);
    overlay_vec(&mut base.dependencies, overlay.dependencies);
    overlay_optional(&mut base.cargo_manifest, overlay.cargo_manifest);
    overlay_optional(&mut base.fabric_manifest, overlay.fabric_manifest);
    overlay_optional(&mut base.runtime_contract, overlay.runtime_contract);
    overlay_optional(&mut base.realtime_bundle, overlay.realtime_bundle);
    base.artifacts.extend(overlay.artifacts);
    overlay_optional(&mut base.rust.cargo_manifest, overlay.rust.cargo_manifest);
    overlay_optional(&mut base.rust.crate_name, overlay.rust.crate_name);
    overlay_vec(&mut base.rust.features, overlay.rust.features);
    if overlay.rust.all_features {
        base.rust.all_features = true;
    }
    if overlay.rust.no_default_features {
        base.rust.no_default_features = true;
    }
    overlay_vec(&mut base.gpu.shader_sources, overlay.gpu.shader_sources);
    overlay_vec(&mut base.gpu.shader_roots, overlay.gpu.shader_roots);
    overlay_vec(&mut base.gpu.compute_keys, overlay.gpu.compute_keys);
    overlay_optional(&mut base.fabric.manifest, overlay.fabric.manifest);
    overlay_optional(&mut base.fabric.entry, overlay.fabric.entry);
    overlay_optional(&mut base.fabric.compute_key, overlay.fabric.compute_key);
}

fn overlay_optional<T>(slot: &mut Option<T>, overlay: Option<T>) {
    if let Some(value) = overlay {
        *slot = Some(value);
    }
}

fn overlay_vec<T>(slot: &mut Vec<T>, overlay: Vec<T>) {
    if !overlay.is_empty() {
        *slot = overlay;
    }
}

fn scan_string_call_chains(
    source: &str,
    function_name: &str,
) -> Vec<(Vec<String>, Vec<(String, Vec<String>, usize)>)> {
    let mut matches = Vec::new();
    let mut offset = 0usize;
    while let Some(call_start) = find_function_call(source, function_name, offset) {
        let function_end = call_start + function_name.len();
        if let Some((args, after_call)) = parse_string_call_arguments(source, function_end) {
            let methods = parse_string_method_chain(source, after_call);
            let next_offset = methods
                .last()
                .map(|(_, _, after)| *after)
                .unwrap_or(after_call);
            matches.push((args, methods));
            offset = next_offset;
        } else {
            offset = function_end;
        }
    }
    matches
}

fn find_function_call(source: &str, function_name: &str, mut offset: usize) -> Option<usize> {
    while let Some(relative) = source[offset..].find(function_name) {
        let start = offset + relative;
        let before = start
            .checked_sub(1)
            .and_then(|index| source.as_bytes().get(index).copied());
        let after = source.as_bytes().get(start + function_name.len()).copied();
        if !matches!(before, Some(byte) if is_identifier_byte(byte))
            && matches!(after, Some(b'(' | b' ' | b'\n' | b'\r' | b'\t'))
        {
            return Some(start);
        }
        offset = start + function_name.len();
    }
    None
}

fn parse_string_method_chain(source: &str, mut index: usize) -> Vec<(String, Vec<String>, usize)> {
    let bytes = source.as_bytes();
    let mut methods = Vec::new();
    loop {
        index = skip_ascii_whitespace(bytes, index);
        if bytes.get(index).copied() != Some(b'.') {
            break;
        }
        index += 1;
        let method_start = index;
        while matches!(bytes.get(index).copied(), Some(byte) if is_identifier_byte(byte)) {
            index += 1;
        }
        if method_start == index {
            break;
        }
        let Some(method) = source.get(method_start..index) else {
            break;
        };
        let Some((args, after_call)) = parse_string_call_arguments(source, index) else {
            break;
        };
        methods.push((method.to_string(), args, after_call));
        index = after_call;
    }
    methods
}

fn parse_string_call_arguments(source: &str, mut index: usize) -> Option<(Vec<String>, usize)> {
    let bytes = source.as_bytes();
    index = skip_ascii_whitespace(bytes, index);
    if bytes.get(index).copied()? != b'(' {
        return None;
    }
    index += 1;
    let mut values = Vec::new();
    loop {
        index = skip_ascii_whitespace(bytes, index);
        match bytes.get(index).copied()? {
            b')' => return Some((values, index + 1)),
            b'"' => {
                let (value, after_string) = parse_quoted_string(source, index)?;
                values.push(value);
                index = skip_ascii_whitespace(bytes, after_string);
                match bytes.get(index).copied()? {
                    b',' => {
                        index += 1;
                    }
                    b')' => return Some((values, index + 1)),
                    _ => return None,
                }
            }
            byte if is_unquoted_literal_start(byte) => {
                let (value, after_literal) = parse_unquoted_literal(source, index)?;
                values.push(value);
                index = skip_ascii_whitespace(bytes, after_literal);
                match bytes.get(index).copied()? {
                    b',' => {
                        index += 1;
                    }
                    b')' => return Some((values, index + 1)),
                    _ => return None,
                }
            }
            _ => return None,
        }
    }
}

fn parse_quoted_string(source: &str, mut index: usize) -> Option<(String, usize)> {
    let bytes = source.as_bytes();
    if bytes.get(index).copied()? != b'"' {
        return None;
    }
    index += 1;
    let mut value = String::new();
    while index < bytes.len() {
        match bytes[index] {
            b'\\' => {
                let escaped = *bytes.get(index + 1)?;
                value.push(match escaped {
                    b'n' => '\n',
                    b'r' => '\r',
                    b't' => '\t',
                    b'"' => '"',
                    b'\\' => '\\',
                    other => other as char,
                });
                index += 2;
            }
            b'"' => return Some((value, index + 1)),
            byte => {
                value.push(byte as char);
                index += 1;
            }
        }
    }
    None
}

fn skip_ascii_whitespace(bytes: &[u8], mut index: usize) -> usize {
    while matches!(bytes.get(index), Some(b' ' | b'\n' | b'\r' | b'\t')) {
        index += 1;
    }
    index
}

fn is_identifier_byte(byte: u8) -> bool {
    byte.is_ascii_alphanumeric() || byte == b'_'
}

fn is_unquoted_literal_start(byte: u8) -> bool {
    byte.is_ascii_digit() || byte == b'-' || byte == b't' || byte == b'f'
}

fn parse_unquoted_literal(source: &str, mut index: usize) -> Option<(String, usize)> {
    let bytes = source.as_bytes();
    let start = index;
    while let Some(byte) = bytes.get(index).copied() {
        if matches!(byte, b',' | b')' | b' ' | b'\n' | b'\r' | b'\t') {
            break;
        }
        index += 1;
    }
    let literal = source.get(start..index)?.trim().to_string();
    if !is_supported_unquoted_literal(&literal) {
        return None;
    }
    Some((literal, index))
}

fn is_supported_unquoted_literal(value: &str) -> bool {
    if matches!(value.trim().to_ascii_lowercase().as_str(), "true" | "false") {
        return true;
    }
    let digits = value.strip_prefix('-').unwrap_or(value);
    !digits.is_empty() && digits.bytes().all(|byte| byte.is_ascii_digit())
}

fn parse_bool_string(value: &str) -> bool {
    matches!(
        value.trim().to_ascii_lowercase().as_str(),
        "1" | "true" | "yes" | "y" | "on"
    )
}

fn assign_first_string(values: &[String], slot: &mut Option<String>) {
    if let Some(value) = values.first() {
        *slot = Some(value.clone());
    }
}

fn assign_first_path(values: &[String], slot: &mut Option<PathBuf>) {
    if let Some(value) = values.first() {
        *slot = Some(PathBuf::from(value));
    }
}

fn push_unique_strings(values: &mut Vec<String>, additions: &[String]) {
    for value in additions {
        if !values.iter().any(|existing| existing == value) {
            values.push(value.clone());
        }
    }
}

fn push_unique_paths_from_strings(values: &mut Vec<PathBuf>, additions: &[String]) {
    for value in additions {
        let path = PathBuf::from(value);
        if !values.iter().any(|existing| existing == &path) {
            values.push(path);
        }
    }
}

fn insert_pair(values: &[String], slot: &mut BTreeMap<String, String>) {
    if values.len() >= 2 {
        slot.insert(values[0].clone(), values[1].clone());
    }
}

fn push_unique_dependency(values: &mut Vec<BladeDependency>, dependency: BladeDependency) {
    if !values
        .iter()
        .any(|existing| normalize_name(&existing.name) == normalize_name(&dependency.name))
    {
        values.push(dependency);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn discovers_explicit_blade_from_default_blades_root() {
        let tmp = tempfile::tempdir().unwrap();
        kfs::write_text(tmp.path().join("Cargo.toml"), "[workspace]\nmembers = []\n").unwrap();
        let blade_root = tmp.path().join("blades").join("fabric");
        kfs::create_dir_all(blade_root.join("src")).unwrap();
        kfs::write_text(
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
        kfs::write_text(
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
    fn manifest_parses_workspace_build_tasks_and_artifact_roots() {
        let tmp = tempfile::tempdir().unwrap();
        let manifest_path = tmp.path().join("KAIN.toml");
        kfs::write_text(
            &manifest_path,
            r#"
[package]
name = "tasked-workspace"

[build]
artifact_root = ".kain/out"
cache_root = ".kain/cache/build"
profile = "release"

[[build.tasks]]
id = "native-filter"
kind = "c"
inputs = ["native/filter.c"]
outputs = ["native/filter.dll"]
depends_on = ["prepare"]
"#,
        )
        .unwrap();

        let manifest = load_kain_manifest(&manifest_path).unwrap();
        assert_eq!(
            manifest.build.artifact_root,
            Some(PathBuf::from(".kain/out"))
        );
        assert_eq!(
            manifest.build.cache_root,
            Some(PathBuf::from(".kain/cache/build"))
        );
        assert_eq!(manifest.build.profile.as_deref(), Some("release"));
        assert_eq!(manifest.build.tasks.len(), 1);
        assert_eq!(manifest.build.tasks[0].kind, "c");
        assert_eq!(manifest.build.tasks[0].depends_on, vec!["prepare"]);
    }

    #[test]
    fn manifest_parses_run_section() {
        let tmp = tempfile::tempdir().unwrap();
        let manifest_path = tmp.path().join("KAIN.toml");
        kfs::write_text(
            &manifest_path,
            r#"
[package]
name = "runnable"

[run]
entry = "src/main.c"
target = "c"
args = ["--demo"]
cwd = "src"
watch = ["src/main.c", "assets"]

[run.env]
KAIN_RUN_MODE = "smoke"
"#,
        )
        .unwrap();

        let manifest = load_kain_manifest(&manifest_path).unwrap();
        assert_eq!(manifest.run.entry, Some(PathBuf::from("src/main.c")));
        assert_eq!(manifest.run.target.as_deref(), Some("c"));
        assert_eq!(manifest.run.args, vec!["--demo"]);
        assert_eq!(manifest.run.cwd, Some(PathBuf::from("src")));
        assert_eq!(manifest.run.watch.len(), 2);
        assert_eq!(
            manifest.run.env.get("KAIN_RUN_MODE").map(String::as_str),
            Some("smoke")
        );
    }

    #[test]
    fn discovers_synthetic_rust_crate_from_crates_root() {
        let tmp = tempfile::tempdir().unwrap();
        kfs::write_text(tmp.path().join("Cargo.toml"), "[workspace]\nmembers = []\n").unwrap();
        let crate_root = tmp.path().join("crates").join("native_math");
        kfs::create_dir_all(crate_root.join("src")).unwrap();
        kfs::write_text(
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
    fn discovers_build_script_only_blade_without_kain_toml() {
        let tmp = tempfile::tempdir().unwrap();
        kfs::write_text(tmp.path().join("Cargo.toml"), "[workspace]\nmembers = []\n").unwrap();
        let blade_root = tmp.path().join("blades").join("constellation");
        kfs::create_dir_all(blade_root.join("src")).unwrap();
        kfs::create_dir_all(blade_root.join("assets")).unwrap();
        kfs::write_text(
            blade_root.join("build.kn"),
            r#"
fn build(ctx: BuildContext) -> BuildGraph:
    let spec = blade("constellation")
        .kind("kain")
        .entry("src/main.kn")
        .source_root("src")
        .build_target("llvm")
        .dependency("kain-json")
    let defaults = build_defaults()
        .profile("release")
    let run = run_defaults()
        .entry("src/main.kn")
        .target("llvm")
        .arg("--demo")
        .watch("assets")
    let check = build_task("script-check")
        .kind("check")
        .entry("src/main.kn")
        .target("llvm")
    return build_graph().task(check)
"#,
        )
        .unwrap();
        kfs::write_text(
            blade_root.join("src").join("main.kn"),
            "fn main() -> Int:\n    return 0\n",
        )
        .unwrap();

        let workspace = discover_workspace(tmp.path()).unwrap();
        let blade = workspace.find_blade("constellation").unwrap();
        assert_eq!(blade.discovery_source, "build-script");
        assert_eq!(blade.kind, "kain");
        assert_eq!(blade.build_targets, vec!["llvm"]);
        assert_eq!(blade.dependencies.len(), 1);
        assert_eq!(blade.dependencies[0].name, "kain-json");
        assert!(blade
            .entry
            .as_ref()
            .unwrap()
            .ends_with(Path::new("src").join("main.kn")));

        let manifest = load_effective_kain_manifest(&blade_root)
            .unwrap()
            .expect("effective manifest");
        assert_eq!(manifest.build.profile.as_deref(), Some("release"));
        assert_eq!(manifest.run.target.as_deref(), Some("llvm"));
        assert_eq!(manifest.run.args, vec!["--demo"]);
        assert_eq!(manifest.build.tasks.len(), 1);
        assert_eq!(manifest.build.tasks[0].id, "script-check");
    }

    #[test]
    fn build_script_manifest_extracts_first_class_std_build_api_tasks() {
        let tmp = tempfile::tempdir().unwrap();
        let blade_root = tmp.path().join("probe");
        kfs::create_dir_all(blade_root.join("src")).unwrap();
        kfs::write_text(
            blade_root.join("build.kn"),
            r#"
use std::build
use std::test
use std::proof
use std::certify

fn build(ctx: BuildContext) -> BuildGraph:
    let spec = blade("probe").entry("src/main.kn").source_root("src").build_target("llvm")
    let check = build_check("check-llvm").entry("src/main.kn").target("llvm")
    let suite = test_suite("source-tests").entry("src/main.kn").requires("check-llvm")
    let proof = proof_obligation("z3-proof")
        .entry("z3/proof.kn")
        .requires("source-tests")
        .requires_capability("target.llvm")
        .axis("target", "llvm")
    let gate = certify_gate("certify").requires("z3-proof").certifies("probe.local")
    return build_graph().blade(spec).task(check).task(suite).task(proof).task(gate)
"#,
        )
        .unwrap();
        kfs::write_text(
            blade_root.join("src").join("main.kn"),
            "fn main() -> Int:\n    return 0\n",
        )
        .unwrap();

        let manifest = load_effective_kain_manifest(&blade_root)
            .unwrap()
            .expect("effective manifest");
        assert_eq!(manifest.build.tasks.len(), 4);
        let by_id = manifest
            .build
            .tasks
            .iter()
            .map(|task| (task.id.as_str(), task))
            .collect::<BTreeMap<_, _>>();
        assert_eq!(by_id["check-llvm"].kind, "check");
        assert_eq!(by_id["source-tests"].kind, "test");
        assert_eq!(
            by_id["source-tests"].depends_on,
            vec!["check-llvm".to_string()]
        );
        assert_eq!(by_id["z3-proof"].kind, "proof");
        assert_eq!(
            by_id["z3-proof"].required_capabilities,
            vec!["target.llvm".to_string()]
        );
        assert_eq!(
            by_id["z3-proof"].matrix_axes,
            vec!["target=llvm".to_string()]
        );
        assert_eq!(by_id["certify"].kind, "certify");
        assert_eq!(by_id["certify"].certifies, vec!["probe.local".to_string()]);
    }

    #[test]
    fn build_script_manifest_extracts_exec_and_amalgamate_tasks() {
        let tmp = tempfile::tempdir().unwrap();
        let blade_root = tmp.path().join("probe");
        kfs::create_dir_all(blade_root.join("src")).unwrap();
        kfs::write_text(
            blade_root.join("build.kn"),
            r#"
use std::build

fn build(ctx: BuildContext) -> BuildGraph:
    let prep = exec_task("refresh-generated")
        .command("cargo")
        .arg("run")
        .arg("-q")
        .env("CARGO_TARGET_DIR", "$root/target/codex-build-graph")
        .stdout("$task/stdout.txt")
        .stderr("$task/stderr.txt")
        .timeout_ms(60000)
        .always_run()
    let capsule = amalgamate_capsule("probe-capsule")
        .path(".")
        .output("$root/.kain/capsules/probe.kn")
        .name("probe")
        .version("0.1.0")
        .tag("portable")
        .meta("album", "probe")
        .storage("editable")
        .contents("source")
        .capsule_set("probe")
        .header("rich")
        .preview_symbols(32)
        .archive(false)
    return build_graph().task(prep).task(capsule)
"#,
        )
        .unwrap();

        let manifest = load_effective_kain_manifest(&blade_root)
            .unwrap()
            .expect("effective manifest");
        assert_eq!(manifest.build.tasks.len(), 2);
        let by_id = manifest
            .build
            .tasks
            .iter()
            .map(|task| (task.id.as_str(), task))
            .collect::<BTreeMap<_, _>>();
        assert_eq!(by_id["refresh-generated"].kind, "exec");
        assert_eq!(
            by_id["refresh-generated"]
                .env
                .get("CARGO_TARGET_DIR")
                .map(String::as_str),
            Some("$root/target/codex-build-graph")
        );
        assert_eq!(
            by_id["refresh-generated"]
                .options
                .get("timeout_ms")
                .map(String::as_str),
            Some("60000")
        );
        assert_eq!(
            by_id["refresh-generated"]
                .options
                .get("always_run")
                .map(String::as_str),
            Some("true")
        );
        assert_eq!(by_id["probe-capsule"].kind, "amalgamate");
        assert_eq!(by_id["probe-capsule"].entry, Some(PathBuf::from(".")));
        assert_eq!(
            by_id["probe-capsule"]
                .options
                .get("storage")
                .map(String::as_str),
            Some("editable")
        );
        assert_eq!(
            by_id["probe-capsule"]
                .options
                .get("contents")
                .map(String::as_str),
            Some("source")
        );
        assert_eq!(
            by_id["probe-capsule"]
                .options
                .get("capsule_set")
                .map(String::as_str),
            Some("probe")
        );
        assert_eq!(
            by_id["probe-capsule"]
                .options
                .get("preview_symbols")
                .map(String::as_str),
            Some("32")
        );
        assert_eq!(by_id["probe-capsule"].tags, vec!["portable".to_string()]);
        assert_eq!(
            by_id["probe-capsule"].meta.get("album").map(String::as_str),
            Some("probe")
        );
    }

    #[test]
    fn build_script_workspace_defaults_can_discover_packages_root() {
        let tmp = tempfile::tempdir().unwrap();
        kfs::write_text(tmp.path().join("Cargo.toml"), "[workspace]\nmembers = []\n").unwrap();
        kfs::write_text(
            tmp.path().join("build.kn"),
            r#"
use std::build

fn build(ctx: BuildContext) -> BuildGraph:
    let ws = workspace_defaults().blade_pattern("packages/*")
    return build_graph().workspace(ws)
"#,
        )
        .unwrap();
        let blade_root = tmp.path().join("packages").join("omni");
        kfs::create_dir_all(blade_root.join("src")).unwrap();
        kfs::write_text(
            blade_root.join("build.kn"),
            r#"
fn build(ctx: BuildContext) -> BuildGraph:
    let spec = blade("omni")
        .kind("kain_library")
        .module_root("src")
    return build_graph()
"#,
        )
        .unwrap();
        kfs::write_text(
            blade_root.join("src").join("omni.kn"),
            "pub fn ready() -> Int:\n    return 1\n",
        )
        .unwrap();

        let workspace = discover_workspace(tmp.path()).unwrap();
        let blade = workspace.find_blade("omni").unwrap();
        assert_eq!(blade.discovery_source, "build-script");
        assert_eq!(blade.kind, "kain_library");
    }

    #[test]
    fn resolves_c_ffi_library_blade_by_library_name() {
        let tmp = tempfile::tempdir().unwrap();
        kfs::write_text(tmp.path().join("Cargo.toml"), "[workspace]\nmembers = []\n").unwrap();
        let blade_root = tmp.path().join("blades").join("native_ops");
        kfs::create_dir_all(blade_root.join("native")).unwrap();
        kfs::write_text(
            blade_root.join("native").join("ops.h"),
            "int add(int a, int b);\n",
        )
        .unwrap();
        kfs::write_text(
            blade_root.join("KAIN.toml"),
            r#"
[package]
name = "native-ops"

[c_ffi]
[[c_ffi.libraries]]
name = "ops"
header = "native/ops.h"
sources = ["native/ops.c"]
"#,
        )
        .unwrap();

        let (blade, library) = resolve_c_ffi_library_blade(tmp.path(), "ops")
            .unwrap()
            .unwrap();
        assert_eq!(blade.name, "native-ops");
        assert_eq!(library.name, "ops");
        assert!(library.header.ends_with("native/ops.h"));
        assert_eq!(library.sources.len(), 1);
    }

    #[test]
    fn infers_nested_module_roots_from_source_tree() {
        let tmp = tempfile::tempdir().unwrap();
        let blade_root = tmp.path().join("blades").join("ui");
        kfs::create_dir_all(blade_root.join("src").join("api")).unwrap();
        kfs::create_dir_all(blade_root.join("src").join("platform").join("desktop")).unwrap();
        kfs::write_text(
            blade_root.join("KAIN.toml"),
            r#"
[package]
name = "ui"

[blade]
entry = "src/main.kn"
source_roots = ["src"]
"#,
        )
        .unwrap();
        kfs::write_text(
            blade_root.join("src").join("main.kn"),
            "fn main() -> Int:\n    return 0\n",
        )
        .unwrap();
        kfs::write_text(
            blade_root.join("src").join("api").join("widgets.kn"),
            "pub fn widgets_ready() -> Int:\n    return 1\n",
        )
        .unwrap();
        kfs::write_text(
            blade_root
                .join("src")
                .join("platform")
                .join("desktop")
                .join("adapter.kn"),
            "pub fn desktop_ready() -> Int:\n    return 1\n",
        )
        .unwrap();

        let workspace = discover_workspace(tmp.path()).unwrap();
        let blade = workspace.find_blade("ui").unwrap();
        assert!(blade
            .module_roots
            .iter()
            .any(|root| root.ends_with(Path::new("src").join("platform").join("desktop"))));
        assert!(blade
            .module_roots
            .iter()
            .any(|root| root.ends_with(Path::new("src").join("api"))));
    }

    #[test]
    fn collects_transitive_c_ffi_libraries_through_dependencies() {
        let tmp = tempfile::tempdir().unwrap();
        kfs::write_text(
            tmp.path().join("KAIN.toml"),
            "[workspace]\nblades = [\"blades/*\"]\n",
        )
        .unwrap();

        let native_root = tmp.path().join("blades").join("native_ops");
        kfs::create_dir_all(native_root.join("native")).unwrap();
        kfs::write_text(
            native_root.join("native").join("ops.h"),
            "int add(int a, int b);\n",
        )
        .unwrap();
        kfs::write_text(
            native_root.join("KAIN.toml"),
            r#"
[package]
name = "native_ops"

[c_ffi]
[[c_ffi.libraries]]
name = "ops"
header = "native/ops.h"
"#,
        )
        .unwrap();

        let app_root = tmp.path().join("blades").join("app");
        kfs::create_dir_all(app_root.join("src")).unwrap();
        kfs::write_text(
            app_root.join("KAIN.toml"),
            r#"
[package]
name = "app"

[blade]
entry = "src/main.kn"
source_roots = ["src"]

[[blade.dependencies]]
name = "native_ops"
"#,
        )
        .unwrap();
        kfs::write_text(
            app_root.join("src").join("main.kn"),
            "fn main() -> Int:\n    return 0\n",
        )
        .unwrap();

        let workspace = discover_workspace(tmp.path()).unwrap();
        let libraries = workspace.transitive_c_ffi_libraries_for("app");
        assert_eq!(libraries.len(), 1);
        assert_eq!(libraries[0].name, "ops");
        assert!(libraries[0].header.ends_with("native/ops.h"));
    }

    #[test]
    fn workspace_manifest_can_override_blade_patterns() {
        let tmp = tempfile::tempdir().unwrap();
        kfs::write_text(
            tmp.path().join("KAIN.toml"),
            "[workspace]\nblades = [\"packages/*\"]\n",
        )
        .unwrap();
        let blade_root = tmp.path().join("packages").join("omni");
        kfs::create_dir_all(blade_root.join("src")).unwrap();
        kfs::write_text(
            blade_root.join("KAIN.toml"),
            "[package]\nname = \"omni\"\n\n[build]\nentry = \"src/main.kn\"\n",
        )
        .unwrap();

        let workspace = discover_workspace(tmp.path()).unwrap();
        assert!(workspace.find_blade("omni").is_some());
    }

    #[test]
    fn discovers_ancestor_workspace_module_roots_from_inside_a_blade() {
        let tmp = tempfile::tempdir().unwrap();
        kfs::write_text(
            tmp.path().join("KAIN.toml"),
            "[workspace]\nblades = [\"blades/*\"]\n",
        )
        .unwrap();

        let shared_root = tmp.path().join("blades").join("shared");
        kfs::create_dir_all(shared_root.join("src")).unwrap();
        kfs::write_text(
            shared_root.join("KAIN.toml"),
            "[package]\nname = \"shared\"\n\n[blade]\nkind = \"kain_library\"\nmodule_roots = [\"src\"]\n",
        )
        .unwrap();
        kfs::write_text(
            shared_root.join("src").join("shared.kn"),
            "pub fn ready() -> Int:\n    return 1\n",
        )
        .unwrap();

        let app_root = tmp.path().join("blades").join("app");
        kfs::create_dir_all(app_root.join("src")).unwrap();
        kfs::write_text(
            app_root.join("KAIN.toml"),
            "[package]\nname = \"app\"\n\n[blade]\nentry = \"src/main.kn\"\nsource_roots = [\"src\"]\nmodule_roots = [\"src\"]\n",
        )
        .unwrap();
        kfs::write_text(
            app_root.join("src").join("main.kn"),
            "fn main() -> Int:\n    return 0\n",
        )
        .unwrap();

        let roots = discover_blade_module_roots_from(app_root.join("src")).unwrap();
        assert!(roots
            .iter()
            .any(|root| root.ends_with(Path::new("blades").join("app").join("src"))));
        assert!(roots
            .iter()
            .any(|root| root.ends_with(Path::new("blades").join("shared").join("src"))));
    }
}
