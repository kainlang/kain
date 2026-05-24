use clap::{Arg, ArgAction, Command};
use std::collections::BTreeMap;

use crate::registry::{CommandArgDefinition, CommandDefinition, CommandRegistry};
use crate::ui::{apply_command_ui, CommandUiPreferences};

#[derive(Debug, Clone)]
pub struct DynamicCommandInvocation {
    pub command: CommandDefinition,
    pub matched_path: Vec<String>,
    pub remaining_args: Vec<String>,
}

#[derive(Debug, Default)]
struct CommandTreeNode {
    command: Option<CommandDefinition>,
    children: BTreeMap<String, CommandTreeNode>,
    visible_aliases: Vec<String>,
}

pub fn dynamic_command_for_bin(registry: &CommandRegistry, bin: &str) -> Command {
    dynamic_command_for_bin_with_ui(
        registry,
        CommandUiPreferences {
            bin,
            theme: crate::ui::CommandUiTheme::Hyperpop,
            color_choice: clap::ColorChoice::Auto,
            experimental_help: false,
        },
    )
}

pub fn dynamic_command_for_bin_with_ui(
    registry: &CommandRegistry,
    preferences: CommandUiPreferences<'_>,
) -> Command {
    let bin = preferences.bin;
    let filtered = registry.for_bin(bin);
    let mut tree = CommandTreeNode::default();
    for command in filtered.commands.iter().filter(|command| !command.hidden) {
        insert_command_path(&mut tree, &command.path, command.clone());
    }
    for command in filtered.commands.iter().filter(|command| !command.hidden) {
        register_visible_aliases(&mut tree, command);
    }

    let about = match bin {
        "blade" => "Standalone Kain blade workspace tool",
        "kn" => "Kain run-first launcher",
        _ => "Kain compiler and toolchain",
    };
    let mut command = Command::new(leak_string(bin.to_string()))
        .about(about)
        .subcommand_required(false)
        .arg_required_else_help(false)
        .allow_external_subcommands(true);

    for (name, child) in tree.children {
        command = command.subcommand(build_clap_node(name, child));
    }
    apply_command_ui(command, preferences)
}

pub fn dynamic_help_for_bin(registry: &CommandRegistry, bin: &str) -> Result<String, String> {
    dynamic_help_for_bin_with_ui(
        registry,
        CommandUiPreferences {
            bin,
            theme: crate::ui::CommandUiTheme::Hyperpop,
            color_choice: clap::ColorChoice::Auto,
            experimental_help: false,
        },
    )
}

pub fn dynamic_help_for_bin_with_ui(
    registry: &CommandRegistry,
    preferences: CommandUiPreferences<'_>,
) -> Result<String, String> {
    let bin = preferences.bin;
    let mut command = dynamic_command_for_bin_with_ui(registry, preferences);
    let mut buffer = Vec::new();
    command
        .write_long_help(&mut buffer)
        .map_err(|err| format!("failed to render dynamic help for {bin}: {err}"))?;
    String::from_utf8(buffer).map_err(|err| format!("dynamic help was not valid UTF-8: {err}"))
}

pub fn resolve_dynamic_invocation(
    registry: &CommandRegistry,
    bin: &str,
    argv: &[String],
) -> Option<DynamicCommandInvocation> {
    let filtered = registry.for_bin(bin);
    let mut best: Option<DynamicCommandInvocation> = None;
    for command in filtered.commands {
        if command.hidden {
            continue;
        }
        for path in command.paths_for_matching() {
            if argv.len() < path.len() {
                continue;
            }
            if argv
                .iter()
                .zip(path.iter())
                .take(path.len())
                .all(|(left, right)| left == right)
            {
                let replace = best
                    .as_ref()
                    .map(|candidate| path.len() > candidate.matched_path.len())
                    .unwrap_or(true);
                if replace {
                    best = Some(DynamicCommandInvocation {
                        remaining_args: argv[path.len()..].to_vec(),
                        command: command.clone(),
                        matched_path: path,
                    });
                }
            }
        }
    }
    best
}

fn insert_command_path(root: &mut CommandTreeNode, path: &[String], command: CommandDefinition) {
    let mut node = root;
    for segment in path {
        node = node.children.entry(segment.clone()).or_default();
    }
    node.command = Some(command);
}

fn register_visible_aliases(root: &mut CommandTreeNode, command: &CommandDefinition) {
    for alias_path in &command.alias_paths {
        if alias_path.len() != command.path.len() || alias_path.is_empty() {
            insert_command_path(&mut *root, alias_path, command.clone());
            continue;
        }
        let parent_len = alias_path.len().saturating_sub(1);
        if alias_path[..parent_len] != command.path[..parent_len] {
            insert_command_path(&mut *root, alias_path, command.clone());
            continue;
        }

        if let Some(leaf_name) = command.path.last() {
            if let Some(parent) = node_mut_at_path(root, &command.path[..parent_len]) {
                let Some(leaf) = parent.children.get_mut(leaf_name) else {
                    continue;
                };
                leaf.visible_aliases
                    .push(alias_path[parent_len].to_string());
            }
        }
    }
}

fn node_mut_at_path<'a>(
    node: &'a mut CommandTreeNode,
    path: &[String],
) -> Option<&'a mut CommandTreeNode> {
    let Some((first, rest)) = path.split_first() else {
        return Some(node);
    };
    let child = node.children.get_mut(first)?;
    node_mut_at_path(child, rest)
}

fn build_clap_node(name: String, node: CommandTreeNode) -> Command {
    let mut command = Command::new(leak_string(name))
        .subcommand_required(false)
        .arg_required_else_help(false)
        .allow_external_subcommands(true);

    if let Some(definition) = &node.command {
        if let Some(about) = &definition.about {
            command = command.about(leak_string(about.clone()));
        }
        if let Some(deprecated) = &definition.deprecated {
            command = command.after_help(leak_string(format!("Deprecated: {deprecated}")));
        }
        for alias in &node.visible_aliases {
            command = command.visible_alias(leak_string(alias.clone()));
        }
        for arg in &definition.args {
            command = command.arg(build_arg(arg));
        }
        command = command.arg(
            Arg::new("__args")
                .num_args(0..)
                .trailing_var_arg(true)
                .allow_hyphen_values(true)
                .hide(true),
        );
    }

    for (child_name, child) in node.children {
        command = command.subcommand(build_clap_node(child_name, child));
    }

    command
}

fn build_arg(definition: &CommandArgDefinition) -> Arg {
    let mut arg = Arg::new(leak_string(definition.name.clone()));
    if definition.positional {
        arg = arg.value_name(leak_string(definition.name.to_ascii_uppercase()));
    } else {
        if let Some(long) = &definition.long {
            arg = arg.long(leak_string(long.clone()));
        }
        if let Some(short) = definition
            .short
            .as_deref()
            .and_then(|value| value.chars().next())
        {
            arg = arg.short(short);
        }
    }

    if matches!(definition.kind.as_str(), "bool" | "flag") {
        arg = arg.action(ArgAction::SetTrue);
    } else {
        arg = arg.num_args(1);
    }

    if let Some(default) = &definition.default {
        arg = arg.default_value(leak_string(default.clone()));
    }
    arg
}

fn leak_string(value: String) -> &'static str {
    Box::leak(value.into_boxed_str())
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::registry::builtin_registry;

    #[test]
    fn dynamic_help_renders_top_level_kain_commands_from_registry() {
        let help = dynamic_help_for_bin(&builtin_registry(), "kain").unwrap();
        assert!(help.contains("Usage: kain"));
        assert!(help.contains("import-c"));
        assert!(help.contains("commands"));
        assert!(help.contains("runtime"));
    }

    #[test]
    fn dynamic_invocation_resolves_longest_builtin_path() {
        let argv = ["run".to_string(), "plan".to_string(), "main.kn".to_string()];
        let invocation = resolve_dynamic_invocation(&builtin_registry(), "kain", &argv).unwrap();
        assert_eq!(invocation.command.id, "run.plan");
        assert_eq!(invocation.remaining_args, ["main.kn"]);
    }
}
