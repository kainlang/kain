use blade::{discover_workspace_root, load_effective_kain_manifest};
use kain_fs as kfs;
use serde::{Deserialize, Serialize};
use std::collections::BTreeSet;
use std::ffi::OsStr;
use std::path::{Path, PathBuf};

const DEFAULT_BUILD_ARTIFACT_ROOT: &str = ".kain/out";
const DEFAULT_BUILD_CACHE_ROOT: &str = ".kain/cache/build";
const DEFAULT_BUILD_REPORT_ROOT: &str = ".kain/reports/build";
const DEFAULT_RUN_CACHE_ROOT: &str = ".kain/cache/run";
const DEFAULT_RUN_REPORT_ROOT: &str = ".kain/reports/run";
const DEFAULT_AMALGAMATE_CACHE_ROOT: &str = ".kain/cache/amalgamate";

pub type CleanResult<T> = Result<T, CleanError>;

#[derive(Debug, thiserror::Error)]
pub enum CleanError {
    #[error("filesystem error: {0}")]
    Fs(#[from] kain_fs::FsError),
    #[error("blade workspace error: {0}")]
    Blade(#[from] blade::BladeError),
    #[error("{0}")]
    Config(String),
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "kebab-case")]
pub enum CleanScope {
    Build,
    Run,
    Amalgamate,
    All,
}

impl CleanScope {
    pub fn parse(value: &str) -> Option<Self> {
        match value.trim().to_ascii_lowercase().as_str() {
            "build" => Some(Self::Build),
            "run" => Some(Self::Run),
            "amalgamate" | "capsule" | "capsules" => Some(Self::Amalgamate),
            "" | "all" => Some(Self::All),
            _ => None,
        }
    }

    pub fn as_str(self) -> &'static str {
        match self {
            Self::Build => "build",
            Self::Run => "run",
            Self::Amalgamate => "amalgamate",
            Self::All => "all",
        }
    }
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq, PartialOrd, Ord)]
#[serde(rename_all = "kebab-case")]
pub enum CleanTargetKind {
    BuildArtifacts,
    BuildCache,
    BuildReports,
    RunCache,
    RunReports,
    AmalgamateCache,
}

impl CleanTargetKind {
    pub fn as_str(self) -> &'static str {
        match self {
            Self::BuildArtifacts => "build-artifacts",
            Self::BuildCache => "build-cache",
            Self::BuildReports => "build-reports",
            Self::RunCache => "run-cache",
            Self::RunReports => "run-reports",
            Self::AmalgamateCache => "amalgamate-cache",
        }
    }
}

#[derive(Debug, Clone)]
pub struct WorkspaceCleanOptions {
    pub path: PathBuf,
    pub scope: CleanScope,
    pub dry_run: bool,
}

impl WorkspaceCleanOptions {
    pub fn new(path: impl Into<PathBuf>) -> Self {
        Self {
            path: path.into(),
            scope: CleanScope::All,
            dry_run: false,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct WorkspaceCleanTarget {
    pub kind: CleanTargetKind,
    pub path: PathBuf,
    pub exists: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct WorkspaceCleanPlan {
    pub workspace_root: PathBuf,
    pub scope: CleanScope,
    pub dry_run: bool,
    pub targets: Vec<WorkspaceCleanTarget>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct WorkspaceCleanAction {
    pub kind: CleanTargetKind,
    pub path: PathBuf,
    pub existed: bool,
    pub removed: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct WorkspaceCleanReport {
    pub workspace_root: PathBuf,
    pub scope: CleanScope,
    pub dry_run: bool,
    pub actions: Vec<WorkspaceCleanAction>,
}

pub fn plan_workspace_clean(options: &WorkspaceCleanOptions) -> CleanResult<WorkspaceCleanPlan> {
    let workspace_root = discover_workspace_root(&options.path)?;
    let manifest = load_effective_kain_manifest(&workspace_root)?;
    let mut targets = Vec::new();

    if matches!(options.scope, CleanScope::Build | CleanScope::All) {
        let build = manifest.as_ref().map(|value| &value.build);
        targets.push(WorkspaceCleanTarget {
            kind: CleanTargetKind::BuildArtifacts,
            path: resolve_workspace_path(
                &workspace_root,
                build
                    .and_then(|value| value.artifact_root.as_deref())
                    .unwrap_or_else(|| Path::new(DEFAULT_BUILD_ARTIFACT_ROOT)),
            ),
            exists: false,
        });
        targets.push(WorkspaceCleanTarget {
            kind: CleanTargetKind::BuildCache,
            path: resolve_workspace_path(
                &workspace_root,
                build
                    .and_then(|value| value.cache_root.as_deref())
                    .unwrap_or_else(|| Path::new(DEFAULT_BUILD_CACHE_ROOT)),
            ),
            exists: false,
        });
        targets.push(WorkspaceCleanTarget {
            kind: CleanTargetKind::BuildReports,
            path: workspace_root.join(DEFAULT_BUILD_REPORT_ROOT),
            exists: false,
        });
    }

    if matches!(options.scope, CleanScope::Run | CleanScope::All) {
        targets.push(WorkspaceCleanTarget {
            kind: CleanTargetKind::RunCache,
            path: workspace_root.join(DEFAULT_RUN_CACHE_ROOT),
            exists: false,
        });
        targets.push(WorkspaceCleanTarget {
            kind: CleanTargetKind::RunReports,
            path: workspace_root.join(DEFAULT_RUN_REPORT_ROOT),
            exists: false,
        });
    }

    if matches!(options.scope, CleanScope::Amalgamate | CleanScope::All) {
        targets.push(WorkspaceCleanTarget {
            kind: CleanTargetKind::AmalgamateCache,
            path: workspace_root.join(DEFAULT_AMALGAMATE_CACHE_ROOT),
            exists: false,
        });
    }

    let mut seen = BTreeSet::new();
    let mut deduped = Vec::new();
    for mut target in targets {
        ensure_safe_clean_root(&workspace_root, &target.path)?;
        if seen.insert(comparable_path(&target.path)) {
            target.exists = target.path.exists();
            deduped.push(target);
        }
    }

    Ok(WorkspaceCleanPlan {
        workspace_root,
        scope: options.scope,
        dry_run: options.dry_run,
        targets: deduped,
    })
}

pub fn execute_workspace_clean(
    options: &WorkspaceCleanOptions,
) -> CleanResult<WorkspaceCleanReport> {
    let plan = plan_workspace_clean(options)?;
    let actions = clean_plan_targets(&plan)?;
    Ok(WorkspaceCleanReport {
        workspace_root: plan.workspace_root,
        scope: plan.scope,
        dry_run: plan.dry_run,
        actions,
    })
}

pub fn ensure_safe_clean_root(workspace_root: &Path, root: &Path) -> CleanResult<()> {
    let workspace_raw = if workspace_root.is_absolute() {
        workspace_root.to_path_buf()
    } else {
        std::env::current_dir()
            .map_err(|error| CleanError::Config(format!("failed to resolve current dir: {error}")))?
            .join(workspace_root)
    };
    let workspace = PathBuf::from(kfs::canonicalize_path(&workspace_raw)?);
    let target_raw = if root.is_absolute() {
        root.to_path_buf()
    } else {
        workspace_raw.join(root)
    };
    let target = kfs::canonicalize_path(&target_raw)
        .map(PathBuf::from)
        .unwrap_or_else(|_| target_raw.clone());
    if paths_equivalent(&target, &workspace)
        || (!path_starts_with_equivalent(&target, &workspace)
            && !path_starts_with_equivalent(&target, &workspace_raw))
    {
        return Err(CleanError::Config(format!(
            "refusing to clean path outside workspace: {}",
            root.display()
        )));
    }
    if !target
        .components()
        .any(|component| component.as_os_str() == OsStr::new(".kain"))
    {
        return Err(CleanError::Config(format!(
            "refusing to clean non-.kain path: {}",
            root.display()
        )));
    }
    Ok(())
}

pub fn clean_paths<I>(workspace_root: &Path, roots: I, dry_run: bool) -> CleanResult<Vec<PathBuf>>
where
    I: IntoIterator<Item = PathBuf>,
{
    let mut removed = Vec::new();
    let mut seen = BTreeSet::new();
    for root in roots {
        ensure_safe_clean_root(workspace_root, &root)?;
        if !seen.insert(comparable_path(&root)) {
            continue;
        }
        if root.exists() {
            if !dry_run {
                kfs::remove_dir_all(&root)?;
            }
            removed.push(root);
        }
    }
    Ok(removed)
}

fn clean_plan_targets(plan: &WorkspaceCleanPlan) -> CleanResult<Vec<WorkspaceCleanAction>> {
    let removed_paths = clean_paths(
        &plan.workspace_root,
        plan.targets.iter().map(|target| target.path.clone()),
        plan.dry_run,
    )?;
    let removed_keys: BTreeSet<String> = removed_paths
        .iter()
        .map(|path| comparable_path(path))
        .collect();
    Ok(plan
        .targets
        .iter()
        .map(|target| WorkspaceCleanAction {
            kind: target.kind,
            path: target.path.clone(),
            existed: target.exists,
            removed: target.exists && removed_keys.contains(&comparable_path(&target.path)),
        })
        .collect())
}

fn resolve_workspace_path(workspace_root: &Path, path: &Path) -> PathBuf {
    if path.is_absolute() {
        path.to_path_buf()
    } else {
        workspace_root.join(path)
    }
}

fn paths_equivalent(left: &Path, right: &Path) -> bool {
    comparable_path(left) == comparable_path(right)
}

fn path_starts_with_equivalent(path: &Path, base: &Path) -> bool {
    path.starts_with(base) || comparable_path(path).starts_with(&comparable_path(base))
}

fn comparable_path(path: &Path) -> String {
    path.display()
        .to_string()
        .trim_start_matches(r"\\?\")
        .replace('\\', "/")
        .to_ascii_lowercase()
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;
    use tempfile::TempDir;

    #[test]
    fn plan_uses_manifest_overrides_for_build_roots() {
        let temp = TempDir::new().expect("temp dir");
        fs::write(
            temp.path().join("KAIN.toml"),
            r#"
[build]
artifact_root = ".kain/out/custom"
cache_root = ".kain/cache/custom"
"#,
        )
        .expect("write manifest");

        let plan =
            plan_workspace_clean(&WorkspaceCleanOptions::new(temp.path())).expect("plan clean");
        assert!(plan.targets.iter().any(|target| {
            target.kind == CleanTargetKind::BuildArtifacts
                && comparable_path(&target.path).ends_with("/.kain/out/custom")
        }));
        assert!(plan.targets.iter().any(|target| {
            target.kind == CleanTargetKind::BuildCache
                && comparable_path(&target.path).ends_with("/.kain/cache/custom")
        }));
    }

    #[test]
    fn clean_rejects_non_kain_roots() {
        let temp = TempDir::new().expect("temp dir");
        let error = ensure_safe_clean_root(temp.path(), &temp.path().join("artifacts"))
            .expect_err("non .kain path should fail");
        assert!(error.to_string().contains("non-.kain"));
    }

    #[test]
    fn dry_run_reports_existing_targets_without_removing_them() {
        let temp = TempDir::new().expect("temp dir");
        fs::write(temp.path().join("KAIN.toml"), "").expect("write manifest");
        let run_cache = temp.path().join(".kain").join("cache").join("run");
        fs::create_dir_all(&run_cache).expect("create run cache");
        fs::write(run_cache.join("probe.txt"), "data").expect("write cached file");

        let mut options = WorkspaceCleanOptions::new(temp.path());
        options.scope = CleanScope::Run;
        options.dry_run = true;
        let report = execute_workspace_clean(&options).expect("run clean");
        assert!(run_cache.exists());
        assert!(report.actions.iter().any(|action| {
            action.kind == CleanTargetKind::RunCache && action.existed && action.removed
        }));
    }
}
