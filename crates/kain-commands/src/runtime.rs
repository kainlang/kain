use serde::Deserialize;
use std::collections::BTreeSet;
use std::path::PathBuf;

use crate::registry::{
    builtin_registry, CommandDefinition, CommandDefinitionSource, CommandRegistry,
};

#[derive(Debug, Clone)]
pub struct RuntimeCommandSource {
    pub label: String,
    pub manifest_path: PathBuf,
}

#[derive(Debug, Clone)]
pub struct RuntimeCommandMatch {
    pub command: CommandDefinition,
    pub matched_path: Vec<String>,
    pub remaining_args: Vec<String>,
}

#[derive(Debug, thiserror::Error)]
pub enum RuntimeCommandError {
    #[error("failed to read command manifest {path}: {source}")]
    Read {
        path: PathBuf,
        source: std::io::Error,
    },
    #[error("failed to parse command manifest {path}: {source}")]
    Parse {
        path: PathBuf,
        source: toml::de::Error,
    },
    #[error("runtime command conflict: {0}")]
    Conflict(String),
}

#[derive(Debug, Deserialize)]
struct RuntimeCommandManifest {
    #[serde(default)]
    commands: Vec<RuntimeCommandRecord>,
}

#[derive(Debug, Deserialize)]
struct RuntimeCommandRecord {
    id: String,
    bins: Vec<String>,
    path: Vec<String>,
    #[serde(default)]
    alias_paths: Vec<Vec<String>>,
    #[serde(default)]
    about: Option<String>,
    handler: String,
    #[serde(default)]
    hidden: bool,
    #[serde(default)]
    deprecated: Option<String>,
}

pub type RuntimeCommandResult<T> = Result<T, RuntimeCommandError>;

pub fn load_runtime_commands(
    sources: &[RuntimeCommandSource],
) -> RuntimeCommandResult<Vec<CommandDefinition>> {
    let mut commands = Vec::new();
    for source in sources {
        let text = std::fs::read_to_string(&source.manifest_path).map_err(|error| {
            RuntimeCommandError::Read {
                path: source.manifest_path.clone(),
                source: error,
            }
        })?;
        let manifest: RuntimeCommandManifest =
            toml::from_str(&text).map_err(|error| RuntimeCommandError::Parse {
                path: source.manifest_path.clone(),
                source: error,
            })?;
        for command in manifest.commands {
            if command.id.trim().is_empty()
                || command.bins.is_empty()
                || command.path.is_empty()
                || command.handler.trim().is_empty()
            {
                continue;
            }
            commands.push(CommandDefinition {
                id: command.id,
                bins: command.bins,
                path: command.path,
                alias_paths: command.alias_paths,
                about: command.about,
                handler: command.handler,
                hidden: command.hidden,
                deprecated: command.deprecated,
                args: Vec::new(),
                source: CommandDefinitionSource {
                    kind: "runtime".to_string(),
                    label: source.label.clone(),
                },
            });
        }
    }
    reject_runtime_conflicts(&commands)?;
    Ok(commands)
}

pub fn runtime_registry(sources: &[RuntimeCommandSource]) -> RuntimeCommandResult<CommandRegistry> {
    Ok(CommandRegistry {
        commands: load_runtime_commands(sources)?,
    })
}

pub fn combined_registry(
    sources: &[RuntimeCommandSource],
) -> RuntimeCommandResult<CommandRegistry> {
    let mut commands = builtin_registry().commands;
    commands.extend(load_runtime_commands(sources)?);
    let registry = CommandRegistry { commands };
    let conflicts = registry.detect_conflicts();
    if let Some(conflict) = conflicts.first() {
        return Err(RuntimeCommandError::Conflict(format!(
            "{} shadows {} on {} {}",
            conflict.second_id,
            conflict.first_id,
            conflict.bin,
            conflict.path.join(" ")
        )));
    }
    Ok(registry)
}

pub fn resolve_runtime_command(
    bin: &str,
    argv: &[String],
    sources: &[RuntimeCommandSource],
) -> RuntimeCommandResult<Option<RuntimeCommandMatch>> {
    let commands = load_runtime_commands(sources)?;
    for command in commands {
        if !command.is_exposed_to_bin(bin) {
            continue;
        }
        for path in command.paths_for_matching() {
            if argv.len() >= path.len()
                && argv
                    .iter()
                    .zip(path.iter())
                    .all(|(left, right)| left == right)
            {
                return Ok(Some(RuntimeCommandMatch {
                    remaining_args: argv[path.len()..].to_vec(),
                    command,
                    matched_path: path,
                }));
            }
        }
    }
    Ok(None)
}

fn reject_runtime_conflicts(commands: &[CommandDefinition]) -> RuntimeCommandResult<()> {
    let builtins = builtin_registry();
    let mut seen_runtime = BTreeSet::<(String, Vec<String>)>::new();
    for command in commands {
        for bin in &command.bins {
            let builtin_paths = builtins.builtin_paths_for_bin(bin);
            for path in command.paths_for_matching() {
                if builtin_paths.contains(&path) {
                    return Err(RuntimeCommandError::Conflict(format!(
                        "runtime command {} cannot shadow builtin {} {}",
                        command.id,
                        bin,
                        path.join(" ")
                    )));
                }
                let key = (bin.clone(), path.clone());
                if !seen_runtime.insert(key) {
                    return Err(RuntimeCommandError::Conflict(format!(
                        "duplicate runtime command path {} {}",
                        bin,
                        path.join(" ")
                    )));
                }
            }
        }
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;

    #[test]
    fn runtime_command_resolves_from_manifest() {
        let dir =
            std::env::temp_dir().join(format!("kain-runtime-command-test-{}", std::process::id()));
        let _ = fs::remove_dir_all(&dir);
        fs::create_dir_all(&dir).unwrap();
        let manifest_path = dir.join("KAIN.toml");
        fs::write(
            &manifest_path,
            r#"
[[commands]]
id = "demo.sharpen"
bins = ["kain"]
path = ["sharpen"]
handler = "blade:demo:sharpen"
"#,
        )
        .unwrap();
        let sources = [RuntimeCommandSource {
            label: "demo".to_string(),
            manifest_path,
        }];
        let found = resolve_runtime_command("kain", &["sharpen".to_string()], &sources)
            .unwrap()
            .unwrap();
        assert_eq!(found.command.id, "demo.sharpen");
        let _ = fs::remove_dir_all(&dir);
    }
}
