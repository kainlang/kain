use std::path::{Path, PathBuf};

pub const KAIN_HOME_ENV_VAR: &str = "KAIN_HOME";
pub const KAIN_CONFIG_ENV_VAR: &str = "KAIN_CONFIG";
pub const KAIN_STDLIB_ENV_VAR: &str = "KAIN_STDLIB_PATH";
pub const KAIN_RUNTIME_C_ENV_VAR: &str = "KAIN_RUNTIME_C_PATH";
pub const KAIN_RUNTIME_MANIFEST_ENV_VARS: &[&str] =
    &["KAIN_RUNTIME_MANIFEST_PATH", "KAIN_RUNTIME_MANIFEST"];
pub const KAIN_CLANG_ENV_VAR: &str = "KAIN_CLANG_PATH";
pub const KAIN_REPO_ROOT_ENV_VAR: &str = "KAIN_REPO_ROOT";

const STDLIB_DIR_NAME: &str = "stdlib";
const KAIN_HOME_DIR_NAME: &str = ".kain";
const KAIN_HOME_SENTINEL_SUFFIXES: &[&str] = &["config.toml", "install_manifest.json"];
const CLANG_CANDIDATE_SUFFIXES: &[&str] = &[
    "toolchain/llvm/bin/clang.exe",
    "toolchain/llvm/bin/clang",
    "third_party/llvm/bin/clang.exe",
    "third_party/llvm/bin/clang",
    "llvm/bin/clang.exe",
    "llvm/bin/clang",
];
const LIBCLANG_CANDIDATE_SUFFIXES: &[&str] = &[
    "toolchain/llvm/bin/libclang.dll",
    "toolchain/llvm/bin/libclang.so",
    "toolchain/llvm/bin/libclang.dylib",
    "third_party/llvm/bin/libclang.dll",
    "third_party/llvm/bin/libclang.so",
    "third_party/llvm/bin/libclang.dylib",
    "llvm/bin/libclang.dll",
    "llvm/bin/libclang.so",
    "llvm/bin/libclang.dylib",
];
const RUNTIME_C_CANDIDATE_SUFFIXES: &[&str] = &[
    "runtime/runtime.c",
    "runtime/kain_runtime.c",
    "runtime/KAIN_runtime.c",
    "src/runtime/c/KAIN_runtime.c",
];
const NATIVE_RUNTIME_MANIFEST_CANDIDATE_SUFFIXES: &[&str] = &[
    "runtime/native_core_runtime.toml",
    "runtime/native_runtime.toml",
    "runtime/native/runtime.toml",
];

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct KainInstallLayout {
    pub home_dir: PathBuf,
    pub bin_dir: PathBuf,
    pub config_path: PathBuf,
    pub stdlib_dir: PathBuf,
    pub runtime_dir: PathBuf,
    pub toolchain_dir: PathBuf,
    pub llvm_bin_dir: PathBuf,
    pub packages_dir: PathBuf,
    pub tooling_dir: PathBuf,
    pub cache_dir: PathBuf,
    pub generated_dir: PathBuf,
    pub install_manifest_path: PathBuf,
}

impl KainInstallLayout {
    pub fn new(home_dir: PathBuf) -> Self {
        let toolchain_dir = home_dir.join("toolchain");
        Self {
            home_dir: home_dir.clone(),
            bin_dir: home_dir.join("bin"),
            config_path: home_dir.join("config.toml"),
            stdlib_dir: home_dir.join(STDLIB_DIR_NAME),
            runtime_dir: home_dir.join("runtime"),
            llvm_bin_dir: toolchain_dir.join("llvm").join("bin"),
            toolchain_dir,
            packages_dir: home_dir.join("packages"),
            tooling_dir: home_dir.join("tooling"),
            cache_dir: home_dir.join("cache"),
            generated_dir: home_dir.join("generated"),
            install_manifest_path: home_dir.join("install_manifest.json"),
        }
    }
}

pub fn canonical_kain_home_dir() -> Option<PathBuf> {
    if let Some(explicit_home) = env_path(KAIN_HOME_ENV_VAR) {
        return Some(explicit_home);
    }

    if let Some(discovered_home) = discover_nearest_kain_home_dir() {
        return Some(discovered_home);
    }

    user_home_dir().map(|home| home.join(KAIN_HOME_DIR_NAME))
}

pub fn default_kain_install_layout() -> Option<KainInstallLayout> {
    canonical_kain_home_dir().map(KainInstallLayout::new)
}

pub fn find_stdlib_search_roots() -> Vec<PathBuf> {
    if let Some(explicit_root) = existing_env_path(KAIN_STDLIB_ENV_VAR) {
        return vec![explicit_root];
    }

    let mut roots = Vec::new();
    for search_root in default_resource_search_roots() {
        let candidate = search_root.join(STDLIB_DIR_NAME);
        if candidate.is_dir() {
            push_unique_path(&mut roots, candidate);
        }
    }
    roots
}

pub fn native_runtime_manifest_candidate_suffixes() -> &'static [&'static str] {
    NATIVE_RUNTIME_MANIFEST_CANDIDATE_SUFFIXES
}

pub fn resolve_runtime_c_path() -> Option<PathBuf> {
    if let Some(explicit_path) = existing_env_path(KAIN_RUNTIME_C_ENV_VAR) {
        return Some(explicit_path);
    }

    find_first_existing_relative_path(
        &default_resource_search_roots(),
        RUNTIME_C_CANDIDATE_SUFFIXES,
    )
}

pub fn resolve_native_runtime_manifest_path() -> Option<PathBuf> {
    if let Some(explicit_path) = existing_env_path_from_any(KAIN_RUNTIME_MANIFEST_ENV_VARS) {
        return Some(explicit_path);
    }

    find_first_existing_relative_path(
        &default_resource_search_roots(),
        NATIVE_RUNTIME_MANIFEST_CANDIDATE_SUFFIXES,
    )
}

pub fn resolve_bundled_clang_path() -> Option<PathBuf> {
    if let Some(explicit_path) = existing_env_path(KAIN_CLANG_ENV_VAR) {
        return Some(explicit_path);
    }

    let suffixes: Vec<&str> = CLANG_CANDIDATE_SUFFIXES
        .iter()
        .copied()
        .filter(|suffix| {
            // On non-Windows hosts, skip .exe candidates so we don't
            // accidentally pick up a Windows clang.exe from a Bazel
            // toolchain that can't resolve Linux DrvFs paths.
            cfg!(windows) || !suffix.ends_with(".exe")
        })
        .collect();

    find_first_existing_relative_path(&default_resource_search_roots(), &suffixes)
}

pub fn resolve_bundled_libclang_path() -> Option<PathBuf> {
    find_first_existing_relative_path(
        &default_resource_search_roots(),
        LIBCLANG_CANDIDATE_SUFFIXES,
    )
}

pub fn apply_windows_msvc_link_env(command: &mut std::process::Command) {
    #[cfg(windows)]
    {
        let mut search_paths = resolve_windows_msvc_link_search_paths();
        for existing in split_env_paths("LIB") {
            push_existing_unique_path(&mut search_paths, existing);
        }
        if !search_paths.is_empty() {
            if let Ok(joined) = std::env::join_paths(search_paths) {
                command.env("LIB", joined);
            }
        }
    }
}

#[cfg(windows)]
pub fn resolve_windows_msvc_link_search_paths() -> Vec<PathBuf> {
    let mut paths = Vec::new();

    if let Some(vc_tools_dir) = existing_env_path("VCToolsInstallDir") {
        push_existing_unique_path(&mut paths, vc_tools_dir.join("lib").join("x64"));
    }

    if let Some(windows_sdk_dir) = env_path("WindowsSdkDir") {
        let version = std::env::var("WindowsSDKLibVersion")
            .ok()
            .map(|value| {
                value
                    .trim()
                    .trim_end_matches('\\')
                    .trim_end_matches('/')
                    .to_string()
            })
            .filter(|value| !value.is_empty());
        append_windows_sdk_lib_dirs(&mut paths, &windows_sdk_dir, version.as_deref());
    }

    append_visual_studio_msvc_lib_dirs(&mut paths);
    append_windows_kits_lib_dirs(&mut paths);

    paths
}

#[cfg(not(windows))]
pub fn resolve_windows_msvc_link_search_paths() -> Vec<PathBuf> {
    Vec::new()
}

pub fn is_path_within_kain_home_bin(path: &Path) -> bool {
    default_kain_install_layout()
        .map(|layout| path.starts_with(layout.bin_dir))
        .unwrap_or(false)
}

fn default_resource_search_roots() -> Vec<PathBuf> {
    let mut roots = Vec::new();

    if let Ok(exe_path) = std::env::current_exe() {
        if let Some(dir) = exe_path.parent() {
            append_ancestor_chain(&mut roots, dir);
        }
    }

    if let Some(layout) = default_kain_install_layout() {
        push_unique_path(&mut roots, layout.home_dir);
    }

    if let Ok(current_dir) = std::env::current_dir() {
        append_ancestor_chain(&mut roots, &current_dir);
    }

    roots
}

fn discover_nearest_kain_home_dir() -> Option<PathBuf> {
    for search_root in kain_home_discovery_roots() {
        if let Some(candidate) = find_kain_home_in_ancestor_chain(&search_root) {
            return Some(candidate);
        }
    }
    None
}

fn kain_home_discovery_roots() -> Vec<PathBuf> {
    let mut roots = Vec::new();

    if let Some(repo_root) = env_path(KAIN_REPO_ROOT_ENV_VAR) {
        push_unique_path(&mut roots, repo_root);
    }

    if let Ok(current_dir) = std::env::current_dir() {
        push_unique_path(&mut roots, current_dir);
    }

    if let Ok(exe_path) = std::env::current_exe() {
        if let Some(exe_dir) = exe_path.parent() {
            push_unique_path(&mut roots, exe_dir.to_path_buf());
        }
    }

    roots
}

fn find_kain_home_in_ancestor_chain(start: &Path) -> Option<PathBuf> {
    let mut cursor = start.to_path_buf();
    loop {
        let candidate = cursor.join(KAIN_HOME_DIR_NAME);
        if looks_like_kain_home(&candidate) {
            return Some(candidate);
        }
        if !cursor.pop() {
            break;
        }
    }
    None
}

fn looks_like_kain_home(path: &Path) -> bool {
    if !path.is_dir() {
        return false;
    }

    KAIN_HOME_SENTINEL_SUFFIXES
        .iter()
        .map(|suffix| path.join(suffix))
        .any(|candidate| candidate.is_file())
}

fn find_first_existing_relative_path(
    search_roots: &[PathBuf],
    suffixes: &[&str],
) -> Option<PathBuf> {
    for root in search_roots {
        for suffix in suffixes {
            let candidate = root.join(suffix);
            if candidate.exists() {
                return Some(candidate);
            }
        }
    }
    None
}

fn append_ancestor_chain(paths: &mut Vec<PathBuf>, start: &Path) {
    let mut cursor = start.to_path_buf();
    loop {
        push_unique_path(paths, cursor.clone());
        if !cursor.pop() {
            break;
        }
    }
}

fn push_unique_path(paths: &mut Vec<PathBuf>, candidate: PathBuf) {
    if !paths.iter().any(|path| path == &candidate) {
        paths.push(candidate);
    }
}

fn env_path(name: &str) -> Option<PathBuf> {
    let raw = std::env::var_os(name)?;
    let trimmed = raw.to_string_lossy().trim().to_string();
    if trimmed.is_empty() {
        return None;
    }
    Some(PathBuf::from(trimmed))
}

#[cfg(windows)]
fn split_env_paths(name: &str) -> Vec<PathBuf> {
    let Some(raw) = std::env::var_os(name) else {
        return Vec::new();
    };
    std::env::split_paths(&raw)
        .filter(|path| !path.as_os_str().is_empty())
        .collect()
}

#[cfg(windows)]
fn push_existing_unique_path(paths: &mut Vec<PathBuf>, candidate: PathBuf) {
    if candidate.is_dir() && !paths.iter().any(|path| path == &candidate) {
        paths.push(candidate);
    }
}

#[cfg(windows)]
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

#[cfg(windows)]
fn append_visual_studio_msvc_lib_dirs(paths: &mut Vec<PathBuf>) {
    let mut roots = Vec::new();
    for root in [
        PathBuf::from(r"C:\Program Files (x86)\Microsoft Visual Studio\2022"),
        PathBuf::from(r"C:\Program Files\Microsoft Visual Studio\2022"),
    ] {
        if root.is_dir() && !roots.iter().any(|value| value == &root) {
            roots.push(root);
        }
    }

    for root in roots {
        for edition in [
            "BuildTools",
            "Community",
            "Professional",
            "Enterprise",
            "Preview",
        ] {
            let tools_root = root.join(edition).join("VC").join("Tools").join("MSVC");
            if let Some(version_dir) = discover_latest_child_dir(&tools_root) {
                push_existing_unique_path(paths, version_dir.join("lib").join("x64"));
            }
        }
    }
}

#[cfg(windows)]
fn append_windows_sdk_lib_dirs(
    paths: &mut Vec<PathBuf>,
    sdk_root: &Path,
    explicit_version: Option<&str>,
) {
    let lib_root = sdk_root.join("Lib");
    if !lib_root.is_dir() {
        return;
    }

    if let Some(version) = explicit_version {
        let version_root = lib_root.join(version);
        if version_root.is_dir() {
            push_existing_unique_path(paths, version_root.join("ucrt").join("x64"));
            push_existing_unique_path(paths, version_root.join("um").join("x64"));
            return;
        }
    }

    if let Some(version_root) = discover_latest_child_dir(&lib_root) {
        push_existing_unique_path(paths, version_root.join("ucrt").join("x64"));
        push_existing_unique_path(paths, version_root.join("um").join("x64"));
    }
}

#[cfg(windows)]
fn append_windows_kits_lib_dirs(paths: &mut Vec<PathBuf>) {
    for root in [
        PathBuf::from(r"C:\Program Files (x86)\Windows Kits\10"),
        PathBuf::from(r"C:\Program Files\Windows Kits\10"),
    ] {
        if root.is_dir() {
            append_windows_sdk_lib_dirs(paths, &root, None);
        }
    }
}

fn existing_env_path(name: &str) -> Option<PathBuf> {
    let candidate = env_path(name)?;
    candidate.exists().then_some(candidate)
}

fn existing_env_path_from_any(names: &[&str]) -> Option<PathBuf> {
    for name in names {
        if let Some(candidate) = existing_env_path(name) {
            return Some(candidate);
        }
    }
    None
}

fn user_home_dir() -> Option<PathBuf> {
    if cfg!(windows) {
        if let Some(path) = env_path("USERPROFILE") {
            return Some(path);
        }
        let home_drive = std::env::var_os("HOMEDRIVE")?;
        let home_path = std::env::var_os("HOMEPATH")?;
        let mut combined = PathBuf::from(home_drive);
        combined.push(home_path);
        return Some(combined);
    }

    env_path("HOME")
}

#[cfg(test)]
mod tests {
    use super::{
        canonical_kain_home_dir, default_kain_install_layout, find_kain_home_in_ancestor_chain,
        looks_like_kain_home, KAIN_HOME_ENV_VAR, KAIN_REPO_ROOT_ENV_VAR,
    };
    use once_cell::sync::Lazy;
    use std::env;
    use std::fs;
    use std::path::PathBuf;
    use std::sync::Mutex;
    use tempfile::TempDir;

    static INSTALL_LAYOUT_TEST_LOCK: Lazy<Mutex<()>> = Lazy::new(|| Mutex::new(()));

    #[test]
    fn explicit_kain_home_env_wins() {
        let _guard = INSTALL_LAYOUT_TEST_LOCK.lock().expect("test lock");
        let previous = env::var_os(KAIN_HOME_ENV_VAR);
        env::set_var(KAIN_HOME_ENV_VAR, "D:/test-kain-home");

        let resolved = canonical_kain_home_dir();

        match previous {
            Some(value) => env::set_var(KAIN_HOME_ENV_VAR, value),
            None => env::remove_var(KAIN_HOME_ENV_VAR),
        }

        assert_eq!(resolved, Some(PathBuf::from("D:/test-kain-home")));
    }

    #[test]
    fn install_layout_derives_standard_directories() {
        let _guard = INSTALL_LAYOUT_TEST_LOCK.lock().expect("test lock");
        let layout = default_kain_install_layout()
            .unwrap_or_else(|| super::KainInstallLayout::new(PathBuf::from("C:/Users/Test/.kain")));
        let explicit_layout = super::KainInstallLayout::new(layout.home_dir.clone());

        assert_eq!(
            explicit_layout.bin_dir,
            explicit_layout.home_dir.join("bin")
        );
        assert_eq!(
            explicit_layout.llvm_bin_dir,
            explicit_layout
                .home_dir
                .join("toolchain")
                .join("llvm")
                .join("bin")
        );
        assert_eq!(
            explicit_layout.install_manifest_path,
            explicit_layout.home_dir.join("install_manifest.json")
        );
    }

    #[test]
    fn ancestor_discovery_prefers_nearest_real_kain_home() {
        let _guard = INSTALL_LAYOUT_TEST_LOCK.lock().expect("test lock");
        let temp_dir = TempDir::new().expect("temp dir");
        let repo_root = temp_dir.path().join("repo");
        let nested = repo_root.join("crates").join("cli");
        fs::create_dir_all(&nested).expect("nested dirs");
        fs::create_dir_all(repo_root.join(".kain")).expect("kain home dir");
        fs::write(repo_root.join(".kain").join("config.toml"), "schema = 1\n").expect("config");

        let discovered = find_kain_home_in_ancestor_chain(&nested).expect("repo-local kain home");
        assert_eq!(discovered, repo_root.join(".kain"));
    }

    #[test]
    fn blade_local_cache_without_control_plane_is_not_treated_as_kain_home() {
        let _guard = INSTALL_LAYOUT_TEST_LOCK.lock().expect("test lock");
        let temp_dir = TempDir::new().expect("temp dir");
        let blade_root = temp_dir.path().join("blade");
        let nested = blade_root.join("src");
        fs::create_dir_all(blade_root.join(".kain").join("out")).expect("blade cache");
        fs::create_dir_all(&nested).expect("nested dirs");

        assert!(!looks_like_kain_home(&blade_root.join(".kain")));
        assert!(find_kain_home_in_ancestor_chain(&nested).is_none());
    }

    #[test]
    fn repo_root_env_allows_real_binary_to_find_repo_local_kain_home() {
        let _guard = INSTALL_LAYOUT_TEST_LOCK.lock().expect("test lock");
        let temp_dir = TempDir::new().expect("temp dir");
        let repo_root = temp_dir.path().join("repo");
        fs::create_dir_all(repo_root.join(".kain")).expect("kain home dir");
        fs::write(repo_root.join(".kain").join("config.toml"), "schema = 1\n").expect("config");

        let previous_repo_root = env::var_os(KAIN_REPO_ROOT_ENV_VAR);
        let previous_home = env::var_os(KAIN_HOME_ENV_VAR);
        env::set_var(KAIN_REPO_ROOT_ENV_VAR, &repo_root);
        env::remove_var(KAIN_HOME_ENV_VAR);

        let resolved = canonical_kain_home_dir();

        match previous_repo_root {
            Some(value) => env::set_var(KAIN_REPO_ROOT_ENV_VAR, value),
            None => env::remove_var(KAIN_REPO_ROOT_ENV_VAR),
        }
        match previous_home {
            Some(value) => env::set_var(KAIN_HOME_ENV_VAR, value),
            None => env::remove_var(KAIN_HOME_ENV_VAR),
        }

        assert_eq!(resolved, Some(repo_root.join(".kain")));
    }
}
