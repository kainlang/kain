use std::{
    fs,
    path::{Path, PathBuf},
};

use regex::Regex;
use serde::Deserialize;

#[derive(Clone, Debug, Deserialize)]
pub struct Fast3dHostConfig {
    #[serde(flatten)]
    pub action: Fast3dHostAction,
}

#[derive(Clone, Debug, Deserialize)]
#[serde(tag = "action", rename_all = "snake_case")]
pub enum Fast3dHostAction {
    Viewer {
        manifest_path: String,
    },
    Snapshot {
        manifest_path: String,
        output_path: String,
        #[serde(default)]
        time_seconds: f32,
    },
    ExtractSm64TitleFace {
        sm64_source_root: String,
        manifest_output_path: String,
    },
}

#[derive(Clone, Debug)]
pub enum ResolvedFast3dHostAction {
    Viewer {
        manifest_path: PathBuf,
    },
    Snapshot {
        manifest_path: PathBuf,
        output_path: PathBuf,
        time_seconds: f32,
    },
    ExtractSm64TitleFace {
        sm64_source_root: PathBuf,
        manifest_output_path: PathBuf,
    },
}

pub fn load_host_config(config_path: &Path) -> Result<Fast3dHostConfig, Box<dyn std::error::Error>> {
    let config_text = fs::read_to_string(config_path)?;
    let extension = config_path
        .extension()
        .and_then(|value| value.to_str())
        .unwrap_or("json")
        .to_ascii_lowercase();
    match extension.as_str() {
        "json" => Ok(serde_json::from_str(&config_text)?),
        "toml" => Ok(toml::from_str(&config_text)?),
        other => Err(format!(
            "unsupported Fast3D host config extension `{other}` for {}",
            config_path.display()
        )
        .into()),
    }
}

impl Fast3dHostConfig {
    pub fn resolve(&self, config_path: &Path) -> Result<ResolvedFast3dHostAction, String> {
        let base_dir = config_path.parent().unwrap_or_else(|| Path::new("."));
        match &self.action {
            Fast3dHostAction::Viewer { manifest_path } => Ok(ResolvedFast3dHostAction::Viewer {
                manifest_path: resolve_config_path(manifest_path, base_dir)?,
            }),
            Fast3dHostAction::Snapshot {
                manifest_path,
                output_path,
                time_seconds,
            } => Ok(ResolvedFast3dHostAction::Snapshot {
                manifest_path: resolve_config_path(manifest_path, base_dir)?,
                output_path: resolve_config_path(output_path, base_dir)?,
                time_seconds: *time_seconds,
            }),
            Fast3dHostAction::ExtractSm64TitleFace {
                sm64_source_root,
                manifest_output_path,
            } => Ok(ResolvedFast3dHostAction::ExtractSm64TitleFace {
                sm64_source_root: resolve_config_path(sm64_source_root, base_dir)?,
                manifest_output_path: resolve_config_path(manifest_output_path, base_dir)?,
            }),
        }
    }
}

fn resolve_config_path(raw_value: &str, base_dir: &Path) -> Result<PathBuf, String> {
    let expanded_value = expand_environment_tokens(raw_value)?;
    let candidate = PathBuf::from(expanded_value);
    if candidate.is_relative() {
        Ok(base_dir.join(candidate))
    } else {
        Ok(candidate)
    }
}

fn expand_environment_tokens(input: &str) -> Result<String, String> {
    let environment_regex = Regex::new(r"\$\{(?P<name>[A-Za-z0-9_]+)\}")
        .map_err(|error| format!("failed to compile Fast3D env regex: {error}"))?;
    let mut expanded = String::new();
    let mut last_end = 0;
    for capture in environment_regex.captures_iter(input) {
        let matched = capture
            .get(0)
            .ok_or("expected Fast3D env token match to exist")?;
        expanded.push_str(&input[last_end..matched.start()]);
        let variable_name = &capture["name"];
        let variable_value = std::env::var(variable_name).map_err(|_| {
            format!("Fast3D host config referenced missing environment variable `{variable_name}`")
        })?;
        expanded.push_str(&variable_value);
        last_end = matched.end();
    }
    expanded.push_str(&input[last_end..]);
    Ok(expanded)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn expands_environment_tokens() {
        std::env::set_var("KAIN_FAST3D_TEST_ROOT", "M:/Code/TestRoot");
        let expanded = expand_environment_tokens("${KAIN_FAST3D_TEST_ROOT}/scene.json").unwrap();
        assert!(expanded.contains("M:/Code/TestRoot"));
    }
}
