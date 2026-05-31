use once_cell::sync::Lazy;
use serde::Deserialize;
use std::path::PathBuf;

const SYSTEM_HEADERS_TOML: &str = include_str!("../system_headers.toml");

static REGISTRY: Lazy<SystemHeaderRegistry> = Lazy::new(|| {
    toml::from_str(SYSTEM_HEADERS_TOML).expect("crates/c-ffi/system_headers.toml must be valid")
});

#[derive(Debug, Clone, Deserialize)]
pub(crate) struct SystemHeaderRegistry {
    #[allow(dead_code)]
    pub(crate) schema_version: u32,
    pub(crate) families: Vec<SystemHeaderFamily>,
}

#[derive(Debug, Clone, Deserialize)]
pub(crate) struct SystemHeaderFamily {
    pub(crate) id: String,
    pub(crate) display_name: String,
    #[serde(default)]
    pub(crate) package_names: Vec<String>,
    #[serde(default)]
    pub(crate) match_exact: Vec<String>,
    #[serde(default)]
    pub(crate) match_extensions: Vec<String>,
    #[serde(default)]
    pub(crate) platforms: Vec<String>,
    #[serde(default)]
    pub(crate) include_roots: Vec<String>,
    #[serde(default)]
    pub(crate) sdk_env_keys: Vec<String>,
    #[serde(default)]
    pub(crate) shim_header: Option<String>,
    #[serde(default)]
    pub(crate) registry_files: Vec<String>,
    #[serde(default)]
    pub(crate) capability_tags: Vec<String>,
    #[serde(default)]
    pub(crate) suppress_named_link_libs_when_import_lib: bool,
    #[serde(default)]
    pub(crate) target_policies: Vec<SystemTargetPolicy>,
    #[serde(default)]
    pub(crate) header_link_policies: Vec<SystemHeaderLinkPolicy>,
}

#[derive(Debug, Clone, Deserialize)]
pub(crate) struct SystemTargetPolicy {
    pub(crate) target: String,
    #[serde(default)]
    pub(crate) link_libs: Vec<String>,
    #[serde(default)]
    pub(crate) toolchain_default_link: bool,
    #[serde(default)]
    pub(crate) dynamic_library_relative_paths: Vec<String>,
    #[serde(default)]
    pub(crate) import_library_relative_paths: Vec<String>,
    #[serde(default)]
    pub(crate) dynamic_library_fallback_paths: Vec<String>,
    #[serde(default)]
    pub(crate) dynamic_library_file_names: Vec<String>,
    #[serde(default)]
    pub(crate) import_library_file_names: Vec<String>,
    #[serde(default)]
    pub(crate) sdk_versioned_parent_paths: Vec<String>,
}

#[derive(Debug, Clone, Deserialize)]
pub(crate) struct SystemHeaderLinkPolicy {
    pub(crate) target: String,
    #[serde(default)]
    pub(crate) header: Option<String>,
    #[serde(default)]
    pub(crate) header_prefix: Option<String>,
    #[serde(default = "default_link_policy_mode")]
    pub(crate) mode: String,
    #[serde(default)]
    pub(crate) link_libs: Vec<String>,
}

fn default_link_policy_mode() -> String {
    "append".to_string()
}

pub(crate) fn registry() -> &'static SystemHeaderRegistry {
    &REGISTRY
}

pub(crate) fn current_target_key() -> &'static str {
    if cfg!(windows) {
        "windows"
    } else if cfg!(target_os = "macos") {
        "macos"
    } else if cfg!(target_os = "linux") {
        "linux"
    } else {
        "other"
    }
}

pub(crate) fn target_key_from_triple(target_triple: &str) -> &'static str {
    if target_triple.contains("windows") {
        "windows"
    } else if target_triple.contains("apple") || target_triple.contains("darwin") {
        "macos"
    } else if target_triple.contains("linux") {
        "linux"
    } else {
        "other"
    }
}

pub(crate) fn family_for_include(target: &str) -> Option<&'static SystemHeaderFamily> {
    let normalized = normalize_header(target);
    let header_name = system_header_file_name(&normalized);
    registry().families.iter().find(|family| {
        family_enabled_for_current_target(family)
            && (matches_exact_header(family, &normalized, header_name)
                || matches_extension(family, header_name))
    })
}

pub(crate) fn family_for_package(package_name: &str) -> Option<&'static SystemHeaderFamily> {
    registry().families.iter().find(|family| {
        family
            .package_names
            .iter()
            .any(|name| name.eq_ignore_ascii_case(package_name))
    })
}

pub(crate) fn system_header_file_name(target: &str) -> &str {
    target.rsplit('/').next().unwrap_or(target)
}

pub(crate) fn supported_family_summary() -> String {
    registry()
        .families
        .iter()
        .filter(|family| family_enabled_for_current_target(family))
        .map(|family| family.display_name.as_str())
        .collect::<Vec<_>>()
        .join(", ")
}

pub(crate) fn current_target_policy(family: &SystemHeaderFamily) -> Option<&SystemTargetPolicy> {
    target_policy(family, current_target_key())
}

pub(crate) fn target_policy<'a>(
    family: &'a SystemHeaderFamily,
    target_key: &str,
) -> Option<&'a SystemTargetPolicy> {
    family
        .target_policies
        .iter()
        .find(|policy| target_matches(&policy.target, target_key))
}

pub(crate) fn link_libs_for_header(family: &SystemHeaderFamily, header_name: &str) -> Vec<String> {
    let target_key = current_target_key();
    let mut libs = target_policy(family, target_key)
        .map(|policy| policy.link_libs.clone())
        .unwrap_or_default();
    let header_name = header_name.to_ascii_lowercase();
    for policy in &family.header_link_policies {
        if !target_matches(&policy.target, target_key) {
            continue;
        }
        if !header_link_policy_matches(policy, &header_name) {
            continue;
        }
        if policy.mode.eq_ignore_ascii_case("replace") {
            libs.clear();
        }
        push_unique_strings(&mut libs, &policy.link_libs);
    }
    libs
}

pub(crate) fn current_target_uses_toolchain_default_link(family: &SystemHeaderFamily) -> bool {
    current_target_policy(family)
        .map(|policy| policy.toolchain_default_link)
        .unwrap_or(false)
}

pub(crate) fn package_sdk_env_keys(package_name: &str) -> Vec<String> {
    if let Some(family) = family_for_package(package_name) {
        return family.sdk_env_keys.clone();
    }
    let normalized = package_env_suffix(package_name);
    vec![format!("KAIN_PLATFORM_{normalized}_SDK")]
}

pub(crate) fn package_header_candidates(package_name: &str) -> Vec<String> {
    family_for_package(package_name)
        .map(|family| family.match_exact.clone())
        .unwrap_or_default()
}

pub(crate) fn package_registry_candidates(package_name: &str) -> Vec<String> {
    family_for_package(package_name)
        .map(|family| family.registry_files.clone())
        .unwrap_or_default()
}

pub(crate) fn package_capability_tags(package_name: &str) -> Vec<String> {
    family_for_package(package_name)
        .map(|family| family.capability_tags.clone())
        .filter(|tags| !tags.is_empty())
        .unwrap_or_else(|| vec!["platform.library.dynamic".to_string()])
}

pub(crate) fn package_library_file_names(
    package_name: &str,
    target_triple: &str,
    import_library: bool,
) -> Option<Vec<String>> {
    let family = family_for_package(package_name)?;
    let target_key = target_key_from_triple(target_triple);
    let policy = target_policy(family, target_key)?;
    let names = if import_library {
        policy.import_library_file_names.clone()
    } else {
        policy.dynamic_library_file_names.clone()
    };
    (!names.is_empty()).then_some(names)
}

pub(crate) fn package_dynamic_library_fallbacks(
    package_name: &str,
    target_triple: &str,
) -> Vec<PathBuf> {
    let Some(family) = family_for_package(package_name) else {
        return Vec::new();
    };
    let target_key = target_key_from_triple(target_triple);
    target_policy(family, target_key)
        .map(|policy| {
            policy
                .dynamic_library_fallback_paths
                .iter()
                .filter_map(|path| expand_path_template(path))
                .collect()
        })
        .unwrap_or_default()
}

pub(crate) fn expand_path_template(template: &str) -> Option<PathBuf> {
    let mut expanded = template.to_string();
    let mut offset = 0usize;
    while let Some(start_rel) = expanded[offset..].find("${env:") {
        let start = offset + start_rel;
        let token_end = expanded[start..].find('}').map(|value| start + value)?;
        let variable = &expanded[start + "${env:".len()..token_end];
        let value = std::env::var(variable).ok()?;
        expanded.replace_range(start..=token_end, &value);
        offset = start + 1;
    }
    Some(PathBuf::from(expanded))
}

fn normalize_header(target: &str) -> String {
    target.replace('\\', "/").to_ascii_lowercase()
}

fn family_enabled_for_current_target(family: &SystemHeaderFamily) -> bool {
    family.platforms.is_empty()
        || family
            .platforms
            .iter()
            .any(|target| target_matches(target, current_target_key()))
}

fn matches_exact_header(
    family: &SystemHeaderFamily,
    normalized_target: &str,
    header_name: &str,
) -> bool {
    family.match_exact.iter().any(|candidate| {
        let candidate = normalize_header(candidate);
        candidate == normalized_target || candidate == header_name
    })
}

fn matches_extension(family: &SystemHeaderFamily, header_name: &str) -> bool {
    family.match_extensions.iter().any(|extension| {
        header_name
            .to_ascii_lowercase()
            .ends_with(&extension.to_ascii_lowercase())
    })
}

fn target_matches(policy_target: &str, target_key: &str) -> bool {
    policy_target.eq_ignore_ascii_case(target_key)
        || policy_target.eq_ignore_ascii_case("all")
        || (policy_target.eq_ignore_ascii_case("unix") && matches!(target_key, "linux" | "macos"))
}

fn header_link_policy_matches(policy: &SystemHeaderLinkPolicy, header_name: &str) -> bool {
    policy
        .header
        .as_deref()
        .map(|header| header.eq_ignore_ascii_case(header_name))
        .unwrap_or(false)
        || policy
            .header_prefix
            .as_deref()
            .map(|prefix| header_name.starts_with(&prefix.to_ascii_lowercase()))
            .unwrap_or(false)
}

fn push_unique_strings(output: &mut Vec<String>, values: &[String]) {
    for value in values {
        if !output.iter().any(|existing| existing == value) {
            output.push(value.clone());
        }
    }
}

fn package_env_suffix(package_name: &str) -> String {
    package_name
        .chars()
        .map(|ch| {
            if ch.is_ascii_alphanumeric() {
                ch.to_ascii_uppercase()
            } else {
                '_'
            }
        })
        .collect()
}
