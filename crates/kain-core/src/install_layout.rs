use std::path::{Path, PathBuf};

pub const KAIN_HOME_ENV_VAR: &str = "KAIN_HOME";
pub const KAIN_STDLIB_ENV_VAR: &str = "KAIN_STDLIB_PATH";
pub const KAIN_RUNTIME_C_ENV_VAR: &str = "KAIN_RUNTIME_C_PATH";
pub const KAIN_RUNTIME_MANIFEST_ENV_VARS: &[&str] =
    &["KAIN_RUNTIME_MANIFEST_PATH", "KAIN_RUNTIME_MANIFEST"];
pub const KAIN_CLANG_ENV_VAR: &str = "KAIN_CLANG_PATH";

const STDLIB_DIR_NAME: &str = "stdlib";
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

    user_home_dir().map(|home| home.join(".kain"))
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

    find_first_existing_relative_path(&default_resource_search_roots(), RUNTIME_C_CANDIDATE_SUFFIXES)
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

    find_first_existing_relative_path(&default_resource_search_roots(), CLANG_CANDIDATE_SUFFIXES)
}

pub fn resolve_bundled_libclang_path() -> Option<PathBuf> {
    find_first_existing_relative_path(
        &default_resource_search_roots(),
        LIBCLANG_CANDIDATE_SUFFIXES,
    )
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

fn find_first_existing_relative_path(search_roots: &[PathBuf], suffixes: &[&str]) -> Option<PathBuf> {
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
    use super::{canonical_kain_home_dir, default_kain_install_layout, KAIN_HOME_ENV_VAR};
    use std::env;
    use std::path::PathBuf;

    #[test]
    fn explicit_kain_home_env_wins() {
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
        let layout = default_kain_install_layout()
            .unwrap_or_else(|| super::KainInstallLayout::new(PathBuf::from("C:/Users/Test/.kain")));
        let explicit_layout = super::KainInstallLayout::new(layout.home_dir.clone());

        assert_eq!(explicit_layout.bin_dir, explicit_layout.home_dir.join("bin"));
        assert_eq!(
            explicit_layout.llvm_bin_dir,
            explicit_layout.home_dir.join("toolchain").join("llvm").join("bin")
        );
        assert_eq!(
            explicit_layout.install_manifest_path,
            explicit_layout.home_dir.join("install_manifest.json")
        );
    }
}
