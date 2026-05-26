use crate::{
    canonicalize_lossy, discover_workspace, discover_workspace_root, existing_directory_anchor,
    load_effective_kain_manifest, names_match, normalize_name, BladeResult, FsFileType,
};
use kain_fs as kfs;
use serde::{Deserialize, Serialize};
use std::collections::BTreeSet;
use std::path::{Path, PathBuf};

pub const KAIN_HOME_ENV_VAR: &str = "KAIN_HOME";
pub const KAIN_LOCKFILE_NAME: &str = "KAIN.lock";
pub const PACKAGE_INDEX_FILE_NAME: &str = "package-index.json";
pub const PACKAGE_INSTALL_FILE_NAME: &str = "package-install.json";

const PACKAGE_STORE_DIR_NAME: &str = "packages";
const PACKAGE_VERSIONS_DIR_NAME: &str = "versions";
const PACKAGE_WORKSPACE_DIR_NAME: &str = "workspace";

#[derive(Debug, Clone, Default, Serialize, Deserialize, PartialEq, Eq)]
#[serde(default)]
pub struct PackageLockfile {
    pub schema: u32,
    pub packages: Vec<LockedPackage>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct LockedPackage {
    pub name: String,
    pub version: String,
    pub source: String,
    pub digest: String,
    pub kind: String,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub capsule_set: Option<String>,
}

impl Default for LockedPackage {
    fn default() -> Self {
        Self {
            name: String::new(),
            version: String::new(),
            source: "kain_home".to_string(),
            digest: String::new(),
            kind: "package".to_string(),
            capsule_set: None,
        }
    }
}

#[derive(Debug, Clone, Default, Serialize, Deserialize, PartialEq, Eq)]
#[serde(default)]
pub struct InstalledPackageIndex {
    pub schema: u32,
    pub name: String,
    pub active_version: Option<String>,
    pub versions: Vec<InstalledPackageVersion>,
}

#[derive(Debug, Clone, Default, Serialize, Deserialize, PartialEq, Eq)]
#[serde(default)]
pub struct InstalledPackageVersion {
    pub version: String,
    pub digest: String,
    pub kind: String,
    pub contents: Vec<String>,
    pub capsule_set: Option<String>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
struct ResolvedInstalledPackage {
    workspace_root: PathBuf,
}

pub fn default_package_store_root() -> Option<PathBuf> {
    explicit_kain_home_dir()
        .or_else(default_user_kain_home_dir)
        .map(|home| home.join(PACKAGE_STORE_DIR_NAME))
}

pub fn package_index_path(package_root: &Path) -> PathBuf {
    package_root.join(PACKAGE_INDEX_FILE_NAME)
}

pub fn package_versions_root(package_root: &Path) -> PathBuf {
    package_root.join(PACKAGE_VERSIONS_DIR_NAME)
}

pub fn package_version_root(package_root: &Path, version: &str) -> PathBuf {
    package_versions_root(package_root).join(version)
}

pub fn package_workspace_root(package_root: &Path, version: &str) -> PathBuf {
    package_version_root(package_root, version).join(PACKAGE_WORKSPACE_DIR_NAME)
}

pub fn package_install_metadata_path(package_root: &Path, version: &str) -> PathBuf {
    package_version_root(package_root, version).join(PACKAGE_INSTALL_FILE_NAME)
}

pub fn package_capsule_path(package_root: &Path, version: &str, contents: &str) -> PathBuf {
    package_version_root(package_root, version).join(format!("{contents}.kn"))
}

pub fn load_package_lockfile(root: &Path) -> BladeResult<Option<PackageLockfile>> {
    let path = root.join(KAIN_LOCKFILE_NAME);
    if !path.exists() {
        return Ok(None);
    }
    let source = kfs::read_text(&path)?;
    let lockfile: PackageLockfile = toml::from_str(&source)?;
    Ok(Some(lockfile))
}

pub fn load_installed_package_index(
    package_root: &Path,
) -> BladeResult<Option<InstalledPackageIndex>> {
    let path = package_index_path(package_root);
    if !path.exists() {
        return Ok(None);
    }
    let source = kfs::read_text(&path)?;
    let index: InstalledPackageIndex = serde_json::from_str(&source).map_err(|err| {
        crate::BladeError::Config(format!(
            "Failed to parse installed package index '{}': {err}",
            path.display()
        ))
    })?;
    Ok(Some(index))
}

pub fn declared_installed_package_workspace_roots_for(
    start: impl AsRef<Path>,
) -> BladeResult<Vec<PathBuf>> {
    let Some(store_root) = default_package_store_root() else {
        return Ok(Vec::new());
    };
    declared_installed_package_workspace_roots_from_store(start, &store_root)
}

pub fn ambient_installed_package_module_roots() -> BladeResult<Vec<PathBuf>> {
    let Some(store_root) = default_package_store_root() else {
        return Ok(Vec::new());
    };
    let packages = ambient_installed_packages(&store_root)?;
    let mut roots = Vec::new();
    for package in packages {
        extend_module_roots_for_workspace(&package.workspace_root, &mut roots)?;
    }
    Ok(roots)
}

fn declared_installed_package_workspace_roots_from_store(
    start: impl AsRef<Path>,
    store_root: &Path,
) -> BladeResult<Vec<PathBuf>> {
    let packages = declared_installed_packages(start.as_ref(), store_root)?;
    Ok(packages
        .into_iter()
        .map(|package| package.workspace_root)
        .collect())
}

fn declared_installed_packages(
    start: &Path,
    store_root: &Path,
) -> BladeResult<Vec<ResolvedInstalledPackage>> {
    if !store_root.exists() || !store_root.is_dir() {
        return Ok(Vec::new());
    }

    let anchor = existing_directory_anchor(start)?;
    let workspace_root = discover_workspace_root(&anchor).unwrap_or(anchor);
    let indices = load_all_package_indices(store_root)?;

    let mut requested = Vec::<(String, Option<String>)>::new();
    if let Some(lockfile) = load_package_lockfile(&workspace_root)? {
        for package in lockfile.packages {
            if package.name.trim().is_empty() {
                continue;
            }
            push_requested_package(&mut requested, &package.name, Some(package.version));
        }
    } else if let Some(manifest) = load_effective_kain_manifest(&workspace_root)? {
        for dependency in manifest.blade.dependencies {
            if dependency.name.trim().is_empty() {
                continue;
            }
            push_requested_package(&mut requested, &dependency.name, None);
        }
    }

    let mut resolved = Vec::new();
    let mut seen = BTreeSet::<PathBuf>::new();
    for (name, version) in requested {
        if let Some(package) = resolve_installed_package(&indices, &name, version.as_deref())? {
            if seen.insert(package.workspace_root.clone()) {
                resolved.push(package);
            }
        }
    }
    Ok(resolved)
}

fn ambient_installed_packages(store_root: &Path) -> BladeResult<Vec<ResolvedInstalledPackage>> {
    if !store_root.exists() || !store_root.is_dir() {
        return Ok(Vec::new());
    }
    let indices = load_all_package_indices(store_root)?;
    let mut resolved = Vec::new();
    let mut seen = BTreeSet::<PathBuf>::new();
    for (package_root, index) in indices {
        let Some(version) = selected_installed_version(&index, None) else {
            continue;
        };
        let workspace_root = package_workspace_root(&package_root, &version.version);
        if !workspace_root.exists() || !workspace_root.is_dir() {
            continue;
        }
        let workspace_root = canonicalize_lossy(&workspace_root);
        if seen.insert(workspace_root.clone()) {
            resolved.push(ResolvedInstalledPackage { workspace_root });
        }
    }
    Ok(resolved)
}

fn load_all_package_indices(
    store_root: &Path,
) -> BladeResult<Vec<(PathBuf, InstalledPackageIndex)>> {
    let mut indices = Vec::new();
    for entry in kfs::read_dir_entries(store_root)? {
        if entry.file_type != FsFileType::Directory {
            continue;
        }
        let Some(index) = load_installed_package_index(&entry.path)? else {
            continue;
        };
        indices.push((canonicalize_lossy(&entry.path), index));
    }
    indices.sort_by(|(left_root, left), (right_root, right)| {
        normalize_name(&left.name)
            .cmp(&normalize_name(&right.name))
            .then(left_root.cmp(right_root))
    });
    Ok(indices)
}

fn resolve_installed_package(
    indices: &[(PathBuf, InstalledPackageIndex)],
    name: &str,
    version: Option<&str>,
) -> BladeResult<Option<ResolvedInstalledPackage>> {
    for (package_root, index) in indices {
        if !names_match(&index.name, name)
            && !package_root
                .file_name()
                .and_then(|value| value.to_str())
                .is_some_and(|value| names_match(value, name))
        {
            continue;
        }

        let Some(selected) = selected_installed_version(index, version) else {
            continue;
        };
        let workspace_root = package_workspace_root(package_root, &selected.version);
        if !workspace_root.exists() || !workspace_root.is_dir() {
            continue;
        }
        return Ok(Some(ResolvedInstalledPackage {
            workspace_root: canonicalize_lossy(&workspace_root),
        }));
    }
    Ok(None)
}

fn selected_installed_version<'a>(
    index: &'a InstalledPackageIndex,
    requested_version: Option<&str>,
) -> Option<&'a InstalledPackageVersion> {
    if let Some(requested_version) = requested_version {
        return index
            .versions
            .iter()
            .find(|candidate| candidate.version == requested_version);
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

fn extend_module_roots_for_workspace(
    workspace_root: &Path,
    roots: &mut Vec<PathBuf>,
) -> BladeResult<()> {
    let workspace = discover_workspace(workspace_root)?;
    for blade in workspace.blades {
        for root in blade.module_roots {
            if root.exists() && !roots.iter().any(|existing| existing == &root) {
                roots.push(root);
            }
        }
    }
    Ok(())
}

fn push_requested_package(
    requested: &mut Vec<(String, Option<String>)>,
    name: &str,
    version: Option<String>,
) {
    let normalized = normalize_name(name);
    if requested.iter().any(|(existing, existing_version)| {
        normalize_name(existing) == normalized && existing_version == &version
    }) {
        return;
    }
    requested.push((name.to_string(), version));
}

fn explicit_kain_home_dir() -> Option<PathBuf> {
    let value = std::env::var_os(KAIN_HOME_ENV_VAR)?;
    let trimmed = value.to_string_lossy().trim().to_string();
    if trimmed.is_empty() {
        return None;
    }
    Some(PathBuf::from(trimmed))
}

fn default_user_kain_home_dir() -> Option<PathBuf> {
    if cfg!(windows) {
        if let Some(path) = env_path("USERPROFILE") {
            return Some(path.join(".kain"));
        }
        let home_drive = std::env::var_os("HOMEDRIVE")?;
        let home_path = std::env::var_os("HOMEPATH")?;
        let mut combined = PathBuf::from(home_drive);
        combined.push(home_path);
        return Some(combined.join(".kain"));
    }
    env_path("HOME").map(|path| path.join(".kain"))
}

fn env_path(name: &str) -> Option<PathBuf> {
    let value = std::env::var_os(name)?;
    let trimmed = value.to_string_lossy().trim().to_string();
    if trimmed.is_empty() {
        return None;
    }
    Some(PathBuf::from(trimmed))
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;
    use tempfile::TempDir;

    #[test]
    fn declared_package_roots_honor_lockfile_versions() {
        let tmp = TempDir::new().unwrap();
        let project_root = tmp.path().join("app");
        let store_root = tmp.path().join("kain-home").join("packages");
        fs::create_dir_all(project_root.join("src")).unwrap();
        fs::create_dir_all(&store_root).unwrap();
        kfs::write_text(
            project_root.join("KAIN.toml"),
            "[package]\nname = \"app\"\n\n[blade]\nentry = \"src/main.kn\"\nmodule_roots = [\"src\"]\n",
        )
        .unwrap();
        kfs::write_text(
            project_root.join(KAIN_LOCKFILE_NAME),
            r#"
schema = 1

[[packages]]
name = "kaintana"
version = "0.2.0"
source = "kain_home"
digest = "sha256:test"
kind = "blade"
"#,
        )
        .unwrap();

        install_fake_package(&store_root, "kaintana", "0.1.0", "legacy", false);
        let expected_root =
            install_fake_package(&store_root, "kaintana", "0.2.0", "preferred", false);

        let roots = declared_installed_package_workspace_roots_from_store(
            project_root.join("src"),
            &store_root,
        )
        .unwrap();

        assert_eq!(roots, vec![expected_root]);
    }

    #[test]
    fn ambient_package_module_roots_use_active_versions() {
        let tmp = TempDir::new().unwrap();
        let home = tmp.path().join("home");
        let store_root = home.join("packages");
        fs::create_dir_all(&store_root).unwrap();
        install_fake_package(&store_root, "alpha", "0.1.0", "alpha", true);
        install_fake_package(&store_root, "beta", "1.0.0", "beta", true);

        let previous = std::env::var_os(KAIN_HOME_ENV_VAR);
        std::env::set_var(KAIN_HOME_ENV_VAR, &home);
        let roots = ambient_installed_package_module_roots().unwrap();
        match previous {
            Some(value) => std::env::set_var(KAIN_HOME_ENV_VAR, value),
            None => std::env::remove_var(KAIN_HOME_ENV_VAR),
        }

        assert_eq!(roots.len(), 2);
        assert!(roots.iter().any(|root| root.ends_with(
            Path::new("alpha")
                .join("versions")
                .join("0.1.0")
                .join("workspace")
                .join("src")
        )));
        assert!(roots.iter().any(|root| root.ends_with(
            Path::new("beta")
                .join("versions")
                .join("1.0.0")
                .join("workspace")
                .join("src")
        )));
    }

    fn install_fake_package(
        store_root: &Path,
        name: &str,
        version: &str,
        module_stem: &str,
        return_src_root: bool,
    ) -> PathBuf {
        let package_root = store_root.join(name);
        let workspace_root = package_workspace_root(&package_root, version);
        let src_root = workspace_root.join("src");
        fs::create_dir_all(&src_root).unwrap();
        kfs::write_text(
            workspace_root.join("KAIN.toml"),
            &format!(
                "[package]\nname = \"{name}\"\nversion = \"{version}\"\n\n[blade]\nkind = \"kain_library\"\nmodule_roots = [\"src\"]\n"
            ),
        )
        .unwrap();
        kfs::write_text(
            src_root.join(format!("{module_stem}.kn")),
            "pub fn ready() -> Int:\n    return 1\n",
        )
        .unwrap();
        let index = InstalledPackageIndex {
            schema: 1,
            name: name.to_string(),
            active_version: Some(version.to_string()),
            versions: vec![InstalledPackageVersion {
                version: version.to_string(),
                digest: format!("sha256:{name}-{version}"),
                kind: "blade".to_string(),
                contents: vec!["source".to_string()],
                capsule_set: Some(name.to_string()),
            }],
        };
        fs::create_dir_all(&package_root).unwrap();
        kfs::write_text(
            package_index_path(&package_root),
            &serde_json::to_string_pretty(&index).unwrap(),
        )
        .unwrap();
        if return_src_root {
            canonicalize_lossy(&workspace_root.join("src"))
        } else {
            canonicalize_lossy(&workspace_root)
        }
    }
}
