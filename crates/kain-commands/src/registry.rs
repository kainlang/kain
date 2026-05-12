use serde::{Deserialize, Serialize};
use std::collections::{BTreeMap, BTreeSet};

use crate::generated::BUILTIN_COMMANDS;

#[derive(Debug, Clone, Copy)]
pub struct BuiltinCommandDefinition {
    pub id: &'static str,
    pub bins: &'static [&'static str],
    pub path: &'static [&'static str],
    pub alias_paths: &'static [&'static [&'static str]],
    pub about: Option<&'static str>,
    pub handler: &'static str,
    pub hidden: bool,
    pub deprecated: Option<&'static str>,
    pub args: &'static [BuiltinCommandArgDefinition],
}

#[derive(Debug, Clone, Copy)]
pub struct BuiltinCommandArgDefinition {
    pub name: &'static str,
    pub kind: &'static str,
    pub positional: bool,
    pub long: Option<&'static str>,
    pub short: Option<&'static str>,
    pub default: Option<&'static str>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct CommandDefinition {
    pub id: String,
    pub bins: Vec<String>,
    pub path: Vec<String>,
    #[serde(default)]
    pub alias_paths: Vec<Vec<String>>,
    pub about: Option<String>,
    pub handler: String,
    #[serde(default)]
    pub hidden: bool,
    #[serde(default)]
    pub deprecated: Option<String>,
    #[serde(default)]
    pub args: Vec<CommandArgDefinition>,
    pub source: CommandDefinitionSource,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct CommandArgDefinition {
    pub name: String,
    pub kind: String,
    #[serde(default)]
    pub positional: bool,
    #[serde(default)]
    pub long: Option<String>,
    #[serde(default)]
    pub short: Option<String>,
    #[serde(default)]
    pub default: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct CommandDefinitionSource {
    pub kind: String,
    pub label: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct CommandRegistry {
    pub commands: Vec<CommandDefinition>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct CommandConflict {
    pub bin: String,
    pub path: Vec<String>,
    pub first_id: String,
    pub second_id: String,
}

impl BuiltinCommandDefinition {
    pub fn to_owned_definition(self) -> CommandDefinition {
        CommandDefinition {
            id: self.id.to_string(),
            bins: self.bins.iter().map(|value| (*value).to_string()).collect(),
            path: self.path.iter().map(|value| (*value).to_string()).collect(),
            alias_paths: self
                .alias_paths
                .iter()
                .map(|path| path.iter().map(|value| (*value).to_string()).collect())
                .collect(),
            about: self.about.map(str::to_string),
            handler: self.handler.to_string(),
            hidden: self.hidden,
            deprecated: self.deprecated.map(str::to_string),
            args: self
                .args
                .iter()
                .map(|arg| CommandArgDefinition {
                    name: arg.name.to_string(),
                    kind: arg.kind.to_string(),
                    positional: arg.positional,
                    long: arg.long.map(str::to_string),
                    short: arg.short.map(str::to_string),
                    default: arg.default.map(str::to_string),
                })
                .collect(),
            source: CommandDefinitionSource {
                kind: "builtin".to_string(),
                label: "kain-commands".to_string(),
            },
        }
    }
}

impl CommandDefinition {
    pub fn paths_for_matching(&self) -> Vec<Vec<String>> {
        let mut paths = Vec::with_capacity(1 + self.alias_paths.len());
        paths.push(self.path.clone());
        paths.extend(self.alias_paths.clone());
        paths
    }

    pub fn is_exposed_to_bin(&self, bin: &str) -> bool {
        self.bins.iter().any(|candidate| candidate == bin)
    }
}

impl CommandRegistry {
    pub fn builtins() -> Self {
        Self {
            commands: builtin_command_definitions(),
        }
    }

    pub fn for_bin(&self, bin: &str) -> Self {
        Self {
            commands: self
                .commands
                .iter()
                .filter(|command| command.is_exposed_to_bin(bin))
                .cloned()
                .collect(),
        }
    }

    pub fn detect_conflicts(&self) -> Vec<CommandConflict> {
        let mut seen = BTreeMap::<(String, Vec<String>), String>::new();
        let mut conflicts = Vec::new();
        for command in &self.commands {
            for bin in &command.bins {
                for path in command.paths_for_matching() {
                    let key = (bin.clone(), path.clone());
                    if let Some(first_id) = seen.get(&key) {
                        conflicts.push(CommandConflict {
                            bin: bin.clone(),
                            path,
                            first_id: first_id.clone(),
                            second_id: command.id.clone(),
                        });
                    } else {
                        seen.insert(key, command.id.clone());
                    }
                }
            }
        }
        conflicts
    }

    pub fn builtin_paths_for_bin(&self, bin: &str) -> BTreeSet<Vec<String>> {
        self.commands
            .iter()
            .filter(|command| command.source.kind == "builtin" && command.is_exposed_to_bin(bin))
            .flat_map(CommandDefinition::paths_for_matching)
            .collect()
    }
}

pub fn builtin_command_definitions() -> Vec<CommandDefinition> {
    BUILTIN_COMMANDS
        .iter()
        .map(|command| command.to_owned_definition())
        .collect()
}

pub fn builtin_registry() -> CommandRegistry {
    CommandRegistry::builtins()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn builtin_registry_has_no_conflicting_paths() {
        let registry = builtin_registry();
        assert_eq!(registry.detect_conflicts(), Vec::new());
    }

    #[test]
    fn registry_exposes_kain_kn_and_blade_views() {
        let registry = builtin_registry();
        assert!(registry
            .for_bin("kain")
            .commands
            .iter()
            .any(|command| command.path == ["build"]));
        assert!(registry
            .for_bin("kn")
            .commands
            .iter()
            .any(|command| command.path == ["run"]));
        assert!(registry
            .for_bin("blade")
            .commands
            .iter()
            .any(|command| command.path == ["build"]));
    }
}
