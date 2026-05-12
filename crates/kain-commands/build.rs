use serde::Deserialize;
use std::fs;
use std::path::{Path, PathBuf};

#[derive(Debug, Deserialize)]
struct CommandManifest {
    #[serde(default)]
    commands: Vec<ManifestCommand>,
}

#[derive(Debug, Deserialize)]
struct ManifestCommand {
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
    #[serde(default)]
    args: Vec<ManifestCommandArg>,
}

#[derive(Debug, Deserialize)]
struct ManifestCommandArg {
    name: String,
    kind: String,
    #[serde(default)]
    positional: bool,
    #[serde(default)]
    long: Option<String>,
    #[serde(default)]
    short: Option<String>,
    #[serde(default)]
    default: Option<String>,
}

fn main() {
    let manifest_dir = PathBuf::from("commands");
    let manifests = [
        manifest_dir.join("kain.toml"),
        manifest_dir.join("blade.toml"),
    ];

    for manifest in &manifests {
        println!("cargo:rerun-if-changed={}", manifest.display());
    }

    let mut commands = Vec::new();
    for manifest_path in &manifests {
        let text = fs::read_to_string(manifest_path)
            .unwrap_or_else(|err| panic!("failed to read {}: {err}", manifest_path.display()));
        let manifest: CommandManifest = toml::from_str(&text)
            .unwrap_or_else(|err| panic!("failed to parse {}: {err}", manifest_path.display()));
        validate_manifest(&manifest_path, &manifest);
        commands.extend(manifest.commands);
    }

    let generated = generate_builtin_commands(&commands);
    let out_path = PathBuf::from(std::env::var("OUT_DIR").unwrap()).join("builtin_commands.rs");
    fs::write(&out_path, generated)
        .unwrap_or_else(|err| panic!("failed to write {}: {err}", out_path.display()));
}

fn validate_manifest(path: &Path, manifest: &CommandManifest) {
    for command in &manifest.commands {
        assert!(
            !command.id.trim().is_empty(),
            "{} contains a command with an empty id",
            path.display()
        );
        assert!(
            !command.bins.is_empty(),
            "{} command {} must expose at least one bin",
            path.display(),
            command.id
        );
        assert!(
            !command.path.is_empty(),
            "{} command {} must have a non-empty path",
            path.display(),
            command.id
        );
        assert!(
            !command.handler.trim().is_empty(),
            "{} command {} must name a handler",
            path.display(),
            command.id
        );
        for arg in &command.args {
            assert!(
                !arg.name.trim().is_empty() && !arg.kind.trim().is_empty(),
                "{} command {} has an invalid arg entry",
                path.display(),
                command.id
            );
        }
    }
}

fn generate_builtin_commands(commands: &[ManifestCommand]) -> String {
    let mut output = String::from("pub const BUILTIN_COMMANDS: &[BuiltinCommandDefinition] = &[\n");
    for command in commands {
        output.push_str("    BuiltinCommandDefinition {\n");
        output.push_str(&format!("        id: {},\n", rust_string(&command.id)));
        output.push_str(&format!(
            "        bins: {},\n",
            rust_string_slice(&command.bins)
        ));
        output.push_str(&format!(
            "        path: {},\n",
            rust_string_slice(&command.path)
        ));
        output.push_str(&format!(
            "        alias_paths: {},\n",
            rust_nested_string_slice(&command.alias_paths)
        ));
        output.push_str(&format!(
            "        about: {},\n",
            rust_optional_string(command.about.as_deref())
        ));
        output.push_str(&format!(
            "        handler: {},\n",
            rust_string(&command.handler)
        ));
        output.push_str(&format!("        hidden: {},\n", command.hidden));
        output.push_str(&format!(
            "        deprecated: {},\n",
            rust_optional_string(command.deprecated.as_deref())
        ));
        output.push_str("        args: &[\n");
        for arg in &command.args {
            output.push_str("            crate::registry::BuiltinCommandArgDefinition {\n");
            output.push_str(&format!(
                "                name: {},\n",
                rust_string(&arg.name)
            ));
            output.push_str(&format!(
                "                kind: {},\n",
                rust_string(&arg.kind)
            ));
            output.push_str(&format!(
                "                positional: {},\n",
                arg.positional
            ));
            output.push_str(&format!(
                "                long: {},\n",
                rust_optional_string(arg.long.as_deref())
            ));
            output.push_str(&format!(
                "                short: {},\n",
                rust_optional_string(arg.short.as_deref())
            ));
            output.push_str(&format!(
                "                default: {},\n",
                rust_optional_string(arg.default.as_deref())
            ));
            output.push_str("            },\n");
        }
        output.push_str("        ],\n");
        output.push_str("    },\n");
    }
    output.push_str("];\n");
    output
}

fn rust_string(value: &str) -> String {
    format!("{value:?}")
}

fn rust_optional_string(value: Option<&str>) -> String {
    match value {
        Some(value) => format!("Some({})", rust_string(value)),
        None => "None".to_string(),
    }
}

fn rust_string_slice(values: &[String]) -> String {
    let values = values
        .iter()
        .map(|value| rust_string(value))
        .collect::<Vec<_>>()
        .join(", ");
    format!("&[{values}]")
}

fn rust_nested_string_slice(values: &[Vec<String>]) -> String {
    let values = values
        .iter()
        .map(|value| rust_string_slice(value))
        .collect::<Vec<_>>()
        .join(", ");
    format!("&[{values}]")
}
