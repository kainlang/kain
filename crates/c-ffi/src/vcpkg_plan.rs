//! vcpkg plan builder: walks `.kn` files, collects versioned includes,
//! deduplicates by port, and emits a `vcpkg.json` manifest.

use crate::port_overrides::header_to_port;
use std::collections::BTreeMap;
use std::path::PathBuf;

/// A single vcpkg dependency derived from versioned includes.
#[derive(Debug, Clone)]
pub struct VcpkgDependency {
    pub port_name: String,
    pub version: String,
    /// Source locations that contributed this constraint.
    pub sources: Vec<VcpkgSourceRef>,
}

/// Tracks where a version constraint came from.
#[derive(Debug, Clone)]
pub struct VcpkgSourceRef {
    pub file: PathBuf,
    pub include_target: String,
    pub version: String,
}

/// The resolved vcpkg plan for a project.
#[derive(Debug, Clone)]
pub struct VcpkgPlan {
    pub dependencies: Vec<VcpkgDependency>,
}

impl VcpkgPlan {
    /// Returns true if this plan has no versioned dependencies.
    pub fn is_empty(&self) -> bool {
        self.dependencies.is_empty()
    }

    /// Generate the vcpkg.json manifest content.
    pub fn to_vcpkg_json(&self, baseline: Option<&str>) -> String {
        let mut json = String::from("{\n");
        json.push_str("  \"name\": \"kain-project\",\n");
        json.push_str("  \"version-string\": \"0.0.0\",\n");

        if !self.dependencies.is_empty() {
            json.push_str("  \"dependencies\": [\n");
            for (i, dep) in self.dependencies.iter().enumerate() {
                json.push_str("    {\n");
                json.push_str(&format!("      \"name\": \"{}\",\n", dep.port_name));
                json.push_str(&format!(
                    "      \"version>=\": \"{}\"\n",
                    dep.version
                ));
                if i + 1 < self.dependencies.len() {
                    json.push_str("    },\n");
                } else {
                    json.push_str("    }\n");
                }
            }
            json.push_str("  ]\n");
        }

        if let Some(sha) = baseline {
            json.push_str(&format!(
                ",\n  \"builtin-baseline\": \"{}\"\n",
                sha
            ));
        }

        json.push_str("}\n");
        json
    }
}

/// Collect versioned includes from a single source string.
///
/// Returns a list of (include_target, version) pairs.
pub fn collect_versioned_includes(source: &str) -> Vec<(String, String)> {
    // Reuse the detection regex from the parent crate
    use once_cell::sync::Lazy;
    use regex::Regex;

    static VERSION_INCLUDE_REGEX: Lazy<Regex> = Lazy::new(|| {
        Regex::new(
            r#"(?m)^\s*include\s+(?:"([^"]+)"|<([^>]+)>)\s+((?:\d+(?:\.\d+)*(?:-[A-Za-z0-9._-]+)?|\d+-\d+-\d+|"[^"]+"))"#,
        )
        .expect("versioned include regex")
    });

    let mut results = Vec::new();
    for cap in VERSION_INCLUDE_REGEX.captures_iter(source) {
        let target = cap
            .get(1)
            .or_else(|| cap.get(2))
            .map(|m| m.as_str().to_string())
            .unwrap_or_default();
        let version = cap
            .get(3)
            .map(|m| m.as_str().trim_matches('"').to_string())
            .unwrap_or_default();
        if !target.is_empty() && !version.is_empty() {
            results.push((target, version));
        }
    }
    results
}

/// Compare two version strings numerically (dotted-decimal), falling back to
/// lexicographic when segments are not parseable as integers.
pub fn version_gt(a: &str, b: &str) -> bool {
    let a_segs: Vec<u64> = a.split('.').filter_map(|s| s.parse().ok()).collect();
    let b_segs: Vec<u64> = b.split('.').filter_map(|s| s.parse().ok()).collect();
    // If both are fully numeric dotted-decimal, compare integer segments
    if a_segs.len() == a.split('.').count() && b_segs.len() == b.split('.').count()
        && !a_segs.is_empty() && !b_segs.is_empty()
    {
        a_segs > b_segs
    } else {
        // Fall back to lexicographic (works for dates, snapshots)
        a > b
    }
}

/// Build a vcpkg plan from collected (include_target, version, source_file) tuples.
pub fn build_plan(
    entries: &[(String, String, PathBuf)],
) -> Result<VcpkgPlan, VcpkgPlanError> {
    let mut by_port: BTreeMap<String, VcpkgDependency> = BTreeMap::new();

    for (include_target, version, source_file) in entries {
        let port = header_to_port(include_target);
        let source_ref = VcpkgSourceRef {
            file: source_file.clone(),
            include_target: include_target.clone(),
            version: version.clone(),
        };

        let entry = by_port.entry(port.clone()).or_insert_with(|| VcpkgDependency {
            port_name: port.clone(),
            version: version.clone(),
            sources: Vec::new(),
        });

        entry.sources.push(source_ref);

        // Pick the highest version using integer-segment comparison.
        // Numeric dotted-decimal versions (e.g. "3.10.0" vs "3.9.0") are
        // compared segment-wise. Non-numeric versions fall back to lexicographic.
        if version_gt(version.as_str(), entry.version.as_str()) {
            entry.version = version.clone();
        }
    }

    Ok(VcpkgPlan {
        dependencies: by_port.into_values().collect(),
    })
}

/// Error from plan building.
#[derive(Debug)]
pub enum VcpkgPlanError {
    /// Two includes for the same port use incompatible version schemes.
    SchemeConflict {
        port: String,
        source_a: PathBuf,
        version_a: String,
        source_b: PathBuf,
        version_b: String,
    },
}

impl std::fmt::Display for VcpkgPlanError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::SchemeConflict {
                port,
                source_a,
                version_a,
                source_b,
                version_b,
            } => write!(
                f,
                "version-scheme conflict for port '{}': {} ({}) vs {} ({})",
                port,
                version_a,
                source_a.display(),
                version_b,
                source_b.display(),
            ),
        }
    }
}

impl std::error::Error for VcpkgPlanError {}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_collect_versioned_includes() {
        let source = r#"
include <sqlite3.h> 3.45.0 as sql
include <openssl/ssl.h> 3.0.8 as ssl
include <math.h> as m
include nuklear as nk
"#;
        let result = collect_versioned_includes(source);
        assert_eq!(result.len(), 2);
        assert_eq!(result[0], ("sqlite3.h".to_string(), "3.45.0".to_string()));
        assert_eq!(result[1], ("openssl/ssl.h".to_string(), "3.0.8".to_string()));
    }

    #[test]
    fn test_build_plan_deduplicates() {
        let entries = vec![
            ("openssl/ssl.h".to_string(), "3.0.8".to_string(), PathBuf::from("a.kn")),
            ("openssl/err.h".to_string(), "3.0.9".to_string(), PathBuf::from("b.kn")),
        ];
        let plan = build_plan(&entries).unwrap();
        assert_eq!(plan.dependencies.len(), 1);
        assert_eq!(plan.dependencies[0].port_name, "openssl");
        assert_eq!(plan.dependencies[0].version, "3.0.9");
    }

    #[test]
    fn test_version_gt_numeric_segments() {
        // Segments are compared as integers, not lexicographically
        assert!(version_gt("3.10.0", "3.9.0"));
        assert!(!version_gt("3.9.0", "3.10.0"));
        assert!(!version_gt("3.9.0", "3.9.0"));
        assert!(version_gt("10.0.0", "9.0.0"));
        // Non-numeric falls back to lexicographic
        assert!(version_gt("2024-02-15", "2024-01-15"));
        assert!(!version_gt("snapshot-20240115", "snapshot-20240116"));
    }

    #[test]
    fn test_vcpkg_json_generation() {
        let plan = VcpkgPlan {
            dependencies: vec![VcpkgDependency {
                port_name: "sqlite3".to_string(),
                version: "3.45.0".to_string(),
                sources: vec![],
            }],
        };
        let json = plan.to_vcpkg_json(None);
        assert!(json.contains("sqlite3"));
        assert!(json.contains("3.45.0"));
    }
}
