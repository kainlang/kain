use crate::error::{KainError, KainResult};
use serde::{Deserialize, Serialize};
use std::collections::BTreeMap;
use std::fs;
use std::path::{Path, PathBuf};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SelfHostSourceProfile {
    #[serde(default = "default_profile_version")]
    pub version: u32,
    #[serde(default = "default_profile_name")]
    pub name: String,
    #[serde(default)]
    pub description: String,
    #[serde(default)]
    pub roots: SelfHostSourceRoots,
    #[serde(default)]
    pub artifacts: SelfHostSourceArtifacts,
    #[serde(default)]
    pub phases: BTreeMap<String, Vec<String>>,
    #[serde(default)]
    pub ownership: SelfHostSourceOwnership,
}

impl Default for SelfHostSourceProfile {
    fn default() -> Self {
        let mut phases = BTreeMap::new();
        phases.insert(
            "phase1".to_string(),
            vec!["kain-core".to_string(), "kain-import".to_string()],
        );
        phases.insert(
            "phase2".to_string(),
            vec![
                "kain-core".to_string(),
                "kain-import".to_string(),
                "cli".to_string(),
                "kain-sys-codegen".to_string(),
            ],
        );
        Self {
            version: default_profile_version(),
            name: default_profile_name(),
            description:
                "Executable-first source-mirror profile for the Rust bootstrap selfhost lane."
                    .to_string(),
            roots: SelfHostSourceRoots::default(),
            artifacts: SelfHostSourceArtifacts::default(),
            phases,
            ownership: SelfHostSourceOwnership::default(),
        }
    }
}

impl SelfHostSourceProfile {
    pub fn load(path: &Path) -> KainResult<Self> {
        let raw = fs::read_to_string(path).map_err(|err| {
            KainError::runtime(format!(
                "Failed to read selfhost source profile {}: {}",
                path.display(),
                err
            ))
        })?;
        serde_json::from_str(&raw).map_err(|err| {
            KainError::runtime(format!(
                "Failed to parse selfhost source profile {}: {}",
                path.display(),
                err
            ))
        })
    }

    pub fn crates_for_phase(&self, phase_name: &str) -> Option<&[String]> {
        self.phases.get(phase_name).map(Vec::as_slice)
    }

    pub fn canonical_source_root(&self, repo_root: &Path) -> PathBuf {
        resolve_profile_path(repo_root, &self.roots.canonical_source_root)
    }

    pub fn output_mirror_root(&self, output_dir: &Path) -> PathBuf {
        resolve_profile_path(output_dir, &self.roots.output_mirror_root)
    }

    pub fn roundtrip_rust_root(&self, output_dir: &Path) -> PathBuf {
        resolve_profile_path(output_dir, &self.roots.roundtrip_rust_root)
    }

    pub fn stage2_workspace_dir(&self, output_dir: &Path) -> PathBuf {
        resolve_profile_path(output_dir, &self.roots.stage2_workspace_dir)
    }

    pub fn source_correspondence_manifest_path(&self, output_dir: &Path) -> PathBuf {
        resolve_profile_path(
            output_dir,
            &self.artifacts.source_correspondence_manifest_file,
        )
    }

    pub fn aggregate_bundle_file_name(&self, crate_name: &str) -> String {
        format!(
            "{crate_name}{}",
            self.artifacts.aggregate_bundle_extension.as_str()
        )
    }

    pub fn aggregate_roundtrip_file_name(&self, crate_name: &str) -> String {
        format!(
            "{crate_name}{}",
            self.artifacts.aggregate_roundtrip_extension.as_str()
        )
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SelfHostSourceRoots {
    #[serde(default = "default_canonical_source_root")]
    pub canonical_source_root: PathBuf,
    #[serde(default = "default_output_mirror_root")]
    pub output_mirror_root: PathBuf,
    #[serde(default = "default_roundtrip_rust_root")]
    pub roundtrip_rust_root: PathBuf,
    #[serde(default = "default_stage2_workspace_dir")]
    pub stage2_workspace_dir: PathBuf,
}

impl Default for SelfHostSourceRoots {
    fn default() -> Self {
        Self {
            canonical_source_root: default_canonical_source_root(),
            output_mirror_root: default_output_mirror_root(),
            roundtrip_rust_root: default_roundtrip_rust_root(),
            stage2_workspace_dir: default_stage2_workspace_dir(),
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SelfHostSourceArtifacts {
    #[serde(default = "default_aggregate_bundle_extension")]
    pub aggregate_bundle_extension: String,
    #[serde(default = "default_aggregate_roundtrip_extension")]
    pub aggregate_roundtrip_extension: String,
    #[serde(default = "default_source_correspondence_manifest_file")]
    pub source_correspondence_manifest_file: PathBuf,
}

impl Default for SelfHostSourceArtifacts {
    fn default() -> Self {
        Self {
            aggregate_bundle_extension: default_aggregate_bundle_extension(),
            aggregate_roundtrip_extension: default_aggregate_roundtrip_extension(),
            source_correspondence_manifest_file: default_source_correspondence_manifest_file(),
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SelfHostSourceOwnership {
    #[serde(default = "default_bootstrap_source")]
    pub bootstrap_source: String,
    #[serde(default = "default_file_ownership_state")]
    pub default_file_ownership_state: String,
    #[serde(default = "default_fallback_mode")]
    pub fallback_mode: String,
    #[serde(default = "default_roundtrip_strategy")]
    pub roundtrip_strategy: String,
    #[serde(default = "default_main_root_strategy")]
    pub synthesized_main_root_strategy: String,
}

impl Default for SelfHostSourceOwnership {
    fn default() -> Self {
        Self {
            bootstrap_source: default_bootstrap_source(),
            default_file_ownership_state: default_file_ownership_state(),
            fallback_mode: default_fallback_mode(),
            roundtrip_strategy: default_roundtrip_strategy(),
            synthesized_main_root_strategy: default_main_root_strategy(),
        }
    }
}

pub fn default_selfhost_source_profile_path(repo_root: &Path) -> PathBuf {
    repo_root
        .join("ouroboros")
        .join("docs")
        .join("selfhost")
        .join("metadata")
        .join("selfhost_source_profile.json")
}

fn resolve_profile_path(base: &Path, configured: &Path) -> PathBuf {
    if configured.is_absolute() {
        configured.to_path_buf()
    } else {
        base.join(configured)
    }
}

fn default_profile_version() -> u32 {
    1
}

fn default_profile_name() -> String {
    "canonical_source_mirror".to_string()
}

fn default_canonical_source_root() -> PathBuf {
    PathBuf::from("src")
}

fn default_output_mirror_root() -> PathBuf {
    PathBuf::from("mirror").join("src")
}

fn default_roundtrip_rust_root() -> PathBuf {
    PathBuf::from("roundtrip_rust")
}

fn default_stage2_workspace_dir() -> PathBuf {
    PathBuf::from("stage2_workspace")
}

fn default_aggregate_bundle_extension() -> String {
    ".kn".to_string()
}

fn default_aggregate_roundtrip_extension() -> String {
    ".roundtrip.rs".to_string()
}

fn default_source_correspondence_manifest_file() -> PathBuf {
    PathBuf::from("source_correspondence_manifest.json")
}

fn default_bootstrap_source() -> String {
    "rust".to_string()
}

fn default_file_ownership_state() -> String {
    "bootstrap_from_rust".to_string()
}

fn default_fallback_mode() -> String {
    "rust_semantic_fallback".to_string()
}

fn default_roundtrip_strategy() -> String {
    "aggregate_bundle_then_split".to_string()
}

fn default_main_root_strategy() -> String {
    "compat_include_lib".to_string()
}

#[cfg(test)]
mod tests {
    use super::{default_selfhost_source_profile_path, SelfHostSourceProfile};
    use std::path::{Path, PathBuf};

    #[test]
    fn default_profile_maps_phase_slices() {
        let profile = SelfHostSourceProfile::default();
        assert_eq!(
            profile.crates_for_phase("phase1").unwrap(),
            ["kain-core", "kain-import"]
        );
        assert_eq!(
            profile.crates_for_phase("phase2").unwrap(),
            ["kain-core", "kain-import", "cli", "kain-sys-codegen"]
        );
    }

    #[test]
    fn default_profile_resolves_relative_roots() {
        let profile = SelfHostSourceProfile::default();
        let repo_root = Path::new("/tmp/kain");
        let output_dir = Path::new("/tmp/out");
        assert_eq!(
            profile.canonical_source_root(repo_root),
            repo_root.join("src")
        );
        assert_eq!(
            profile.output_mirror_root(output_dir),
            output_dir.join("mirror").join("src")
        );
        assert_eq!(
            profile.roundtrip_rust_root(output_dir),
            output_dir.join("roundtrip_rust")
        );
    }

    #[test]
    fn default_profile_path_lives_under_ouroboros_metadata() {
        let repo_root = PathBuf::from("/tmp/kain");
        assert_eq!(
            default_selfhost_source_profile_path(&repo_root),
            repo_root
                .join("ouroboros")
                .join("docs")
                .join("selfhost")
                .join("metadata")
                .join("selfhost_source_profile.json")
        );
    }
}
