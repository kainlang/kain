use std::collections::{BTreeMap, BTreeSet};
use std::env;
use std::path::{Path, PathBuf};

use crate::{absolute_path, normalize_path, FsError, FsErrorKind, FsResult};

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub enum FsCapability {
    Read,
    Write,
    List,
    Metadata,
    Delete,
    Temp,
    Watch,
    Transaction,
    Mount,
}

impl FsCapability {
    pub fn as_str(self) -> &'static str {
        match self {
            FsCapability::Read => "fs.read",
            FsCapability::Write => "fs.write",
            FsCapability::List => "fs.list",
            FsCapability::Metadata => "fs.metadata",
            FsCapability::Delete => "fs.delete",
            FsCapability::Temp => "fs.temp",
            FsCapability::Watch => "fs.watch",
            FsCapability::Transaction => "fs.transaction",
            FsCapability::Mount => "fs.mount",
        }
    }

    pub fn from_str(value: &str) -> Option<Self> {
        match value.trim() {
            "read" | "fs.read" => Some(Self::Read),
            "write" | "fs.write" => Some(Self::Write),
            "list" | "fs.list" => Some(Self::List),
            "metadata" | "fs.metadata" => Some(Self::Metadata),
            "delete" | "fs.delete" => Some(Self::Delete),
            "temp" | "fs.temp" => Some(Self::Temp),
            "watch" | "fs.watch" => Some(Self::Watch),
            "transaction" | "fs.transaction" => Some(Self::Transaction),
            "mount" | "fs.mount" => Some(Self::Mount),
            _ => None,
        }
    }

    pub fn all() -> impl Iterator<Item = Self> {
        [
            Self::Read,
            Self::Write,
            Self::List,
            Self::Metadata,
            Self::Delete,
            Self::Temp,
            Self::Watch,
            Self::Transaction,
            Self::Mount,
        ]
        .into_iter()
    }
}

#[derive(Debug, Clone)]
pub struct FsMount {
    pub key: String,
    pub root: PathBuf,
    pub read_only: bool,
}

#[derive(Debug, Clone)]
pub struct FsSandbox {
    mounts: BTreeMap<String, FsMount>,
    grants: BTreeSet<FsCapability>,
    allow_host_paths: bool,
}

impl FsSandbox {
    pub fn unrestricted_project() -> Self {
        let project_root = env::current_dir().unwrap_or_else(|_| PathBuf::from("."));
        let mut sandbox = Self {
            mounts: BTreeMap::new(),
            grants: FsCapability::all().collect(),
            allow_host_paths: true,
        };
        sandbox.mount("project", project_root, false);
        sandbox.mount("temp", env::temp_dir(), false);
        if let Ok(runtime_root) = env::var("KAIN_RUNTIME_ROOT") {
            sandbox.mount("runtime", runtime_root, true);
        }
        let cache_root = env::var("KAIN_CACHE_DIR")
            .map(PathBuf::from)
            .unwrap_or_else(|_| PathBuf::from(".kain").join("cache"));
        sandbox.mount("cache", cache_root, false);
        sandbox
    }

    pub fn project_sandbox(project_root: impl Into<PathBuf>) -> Self {
        let mut sandbox = Self {
            mounts: BTreeMap::new(),
            grants: BTreeSet::from([
                FsCapability::Read,
                FsCapability::Write,
                FsCapability::List,
                FsCapability::Metadata,
                FsCapability::Delete,
                FsCapability::Temp,
                FsCapability::Watch,
                FsCapability::Transaction,
            ]),
            allow_host_paths: false,
        };
        sandbox.mount("project", project_root, false);
        sandbox.mount("temp", env::temp_dir(), false);
        sandbox
    }

    pub fn mount(&mut self, key: impl Into<String>, root: impl Into<PathBuf>, read_only: bool) {
        let key = normalize_mount_key(key.into());
        self.mounts.insert(
            key.clone(),
            FsMount {
                key,
                root: root.into(),
                read_only,
            },
        );
    }

    pub fn unmount(&mut self, key: &str) -> bool {
        self.mounts.remove(&normalize_mount_key(key)).is_some()
    }

    pub fn grant(&mut self, capability: FsCapability) {
        self.grants.insert(capability);
    }

    pub fn revoke(&mut self, capability: FsCapability) {
        self.grants.remove(&capability);
    }

    pub fn has_capability(&self, capability: FsCapability) -> bool {
        self.grants.contains(&capability)
    }

    pub fn allow_host_paths(&mut self, allow: bool) {
        self.allow_host_paths = allow;
    }

    pub fn mounted_roots(&self) -> Vec<FsMount> {
        self.mounts.values().cloned().collect()
    }

    pub fn resolve(&self, authored_path: impl AsRef<str>) -> FsResult<PathBuf> {
        let authored_path = authored_path.as_ref();
        if let Some((key, rest)) = authored_path
            .strip_prefix("fs://")
            .and_then(split_virtual_path)
        {
            let Some(mount) = self.mounts.get(&normalize_mount_key(key)) else {
                return Err(FsError::new(
                    "resolve_virtual_path",
                    authored_path,
                    FsErrorKind::NotFound,
                    format!("unknown virtual filesystem root '{key}'"),
                ));
            };
            return Ok(clean_join(&mount.root, rest));
        }

        let path = PathBuf::from(authored_path);
        if self.allow_host_paths {
            return Ok(path);
        }

        let Some(project) = self.mounts.get("project") else {
            return Err(FsError::new(
                "resolve_path",
                authored_path,
                FsErrorKind::AccessDenied,
                "host paths are disabled and no fs://project root is mounted",
            ));
        };
        Ok(clean_join(&project.root, authored_path))
    }

    pub fn authorize(
        &self,
        capability: FsCapability,
        authored_path: impl AsRef<str>,
    ) -> FsResult<PathBuf> {
        let authored_path = authored_path.as_ref();
        if !self.has_capability(capability) {
            return Err(FsError::new(
                "authorize_path",
                authored_path,
                FsErrorKind::AccessDenied,
                format!("missing required capability '{}'", capability.as_str()),
            ));
        }
        let resolved = self.resolve(authored_path)?;
        if let Some((key, _)) = authored_path
            .strip_prefix("fs://")
            .and_then(split_virtual_path)
        {
            if let Some(mount) = self.mounts.get(&normalize_mount_key(key)) {
                if mount.read_only
                    && matches!(
                        capability,
                        FsCapability::Write | FsCapability::Delete | FsCapability::Transaction
                    )
                {
                    return Err(FsError::new(
                        "authorize_path",
                        authored_path,
                        FsErrorKind::AccessDenied,
                        format!("virtual root '{key}' is read-only"),
                    ));
                }
            }
        }
        Ok(resolved)
    }

    pub fn describe(&self) -> String {
        let mut lines = Vec::new();
        lines.push(format!("allow_host_paths={}", self.allow_host_paths));
        let capabilities = self
            .grants
            .iter()
            .map(|capability| capability.as_str())
            .collect::<Vec<_>>()
            .join(",");
        lines.push(format!("capabilities={capabilities}"));
        for mount in self.mounts.values() {
            lines.push(format!(
                "mount={}:{}:{}",
                mount.key,
                normalize_path(&mount.root),
                if mount.read_only {
                    "read_only"
                } else {
                    "read_write"
                }
            ));
        }
        lines.join("\n")
    }
}

fn split_virtual_path(path: &str) -> Option<(&str, &str)> {
    let mut parts = path.splitn(2, '/');
    let key = parts.next()?;
    let rest = parts.next().unwrap_or_default();
    Some((key, rest))
}

fn normalize_mount_key(key: impl AsRef<str>) -> String {
    key.as_ref()
        .trim()
        .trim_start_matches("fs://")
        .trim_matches('/')
        .to_ascii_lowercase()
}

fn clean_join(root: &Path, rest: impl AsRef<Path>) -> PathBuf {
    let rest = rest.as_ref();
    if rest.as_os_str().is_empty() {
        return root.to_path_buf();
    }
    let joined = if rest.is_absolute() {
        rest.to_path_buf()
    } else {
        root.join(rest)
    };
    absolute_path(&joined).map(PathBuf::from).unwrap_or(joined)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn resolves_virtual_project_paths_and_checks_capabilities() {
        let temp = tempfile::tempdir().expect("tempdir");
        let mut sandbox = FsSandbox::project_sandbox(temp.path());
        let resolved = sandbox
            .authorize(FsCapability::Read, "fs://project/src/main.kn")
            .expect("resolve");
        assert!(resolved.ends_with(Path::new("src").join("main.kn")));

        sandbox.revoke(FsCapability::Read);
        let error = sandbox
            .authorize(FsCapability::Read, "fs://project/src/main.kn")
            .expect_err("missing cap");
        assert_eq!(error.kind, FsErrorKind::AccessDenied);
    }

    #[test]
    fn read_only_mount_rejects_writes() {
        let temp = tempfile::tempdir().expect("tempdir");
        let mut sandbox = FsSandbox::project_sandbox(temp.path());
        sandbox.mount("runtime", temp.path(), true);
        let error = sandbox
            .authorize(FsCapability::Write, "fs://runtime/native_runtime.toml")
            .expect_err("read only");
        assert_eq!(error.kind, FsErrorKind::AccessDenied);
    }
}
