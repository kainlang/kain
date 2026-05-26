use serde::Deserialize;
use std::collections::BTreeSet;
use std::fs;
use std::path::{Path, PathBuf};

#[derive(Debug, Deserialize)]
struct CommandIndex {
    #[serde(default)]
    packs: Vec<CommandPackIndexEntry>,
}

#[derive(Debug, Deserialize)]
struct CommandPackIndexEntry {
    id: String,
    path: PathBuf,
}

#[derive(Debug, Deserialize)]
struct CommandManifest {
    pack: CommandPackManifest,
    #[serde(default)]
    commands: Vec<ManifestCommand>,
}

#[derive(Debug, Deserialize)]
struct CommandPackManifest {
    id: String,
    title: String,
    #[serde(default)]
    owner: Option<String>,
    #[serde(default)]
    about: Option<String>,
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
    tags: Vec<String>,
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

#[derive(Debug)]
struct LoadedCommandPack {
    pack: CommandPackManifest,
    commands: Vec<ManifestCommand>,
}

fn main() {
    let manifest_dir = PathBuf::from("commands");
    let index_path = manifest_dir.join("index.toml");
    println!("cargo:rerun-if-changed={}", index_path.display());

    let index_text = fs::read_to_string(&index_path)
        .unwrap_or_else(|err| panic!("failed to read {}: {err}", index_path.display()));
    let index: CommandIndex = toml::from_str(&index_text)
        .unwrap_or_else(|err| panic!("failed to parse {}: {err}", index_path.display()));
    validate_index(&index_path, &index);

    let mut packs = Vec::new();
    for pack_entry in &index.packs {
        let pack_path = manifest_dir.join(&pack_entry.path);
        println!("cargo:rerun-if-changed={}", pack_path.display());
        let text = fs::read_to_string(&pack_path)
            .unwrap_or_else(|err| panic!("failed to read {}: {err}", pack_path.display()));
        let manifest: CommandManifest = toml::from_str(&text)
            .unwrap_or_else(|err| panic!("failed to parse {}: {err}", pack_path.display()));
        validate_manifest(&pack_path, pack_entry, &manifest);
        packs.push(LoadedCommandPack {
            pack: manifest.pack,
            commands: manifest.commands,
        });
    }

    validate_loaded_packs(&packs);
    let generated = generate_builtin_commands(&packs);
    let out_path = PathBuf::from(std::env::var("OUT_DIR").unwrap()).join("builtin_commands.rs");
    fs::write(&out_path, generated)
        .unwrap_or_else(|err| panic!("failed to write {}: {err}", out_path.display()));
}

fn validate_index(path: &Path, index: &CommandIndex) {
    assert!(
        !index.packs.is_empty(),
        "{} must declare at least one command pack",
        path.display()
    );
    let mut ids = BTreeSet::new();
    let mut paths = BTreeSet::new();
    for pack in &index.packs {
        assert!(
            !pack.id.trim().is_empty(),
            "{} contains a pack with an empty id",
            path.display()
        );
        assert!(
            ids.insert(pack.id.clone()),
            "{} declares duplicate pack id {}",
            path.display(),
            pack.id
        );
        assert!(
            paths.insert(pack.path.clone()),
            "{} declares duplicate pack path {}",
            path.display(),
            pack.path.display()
        );
    }
}

fn validate_manifest(path: &Path, entry: &CommandPackIndexEntry, manifest: &CommandManifest) {
    assert_eq!(
        manifest.pack.id,
        entry.id,
        "{} pack id must match index entry {}",
        path.display(),
        entry.id
    );
    assert!(
        !manifest.pack.title.trim().is_empty(),
        "{} pack {} must have a title",
        path.display(),
        manifest.pack.id
    );
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

fn validate_loaded_packs(packs: &[LoadedCommandPack]) {
    let mut command_ids = BTreeSet::new();
    for pack in packs {
        for command in &pack.commands {
            assert!(
                command_ids.insert(command.id.clone()),
                "duplicate builtin command id {}",
                command.id
            );
        }
    }
}

fn generate_builtin_commands(packs: &[LoadedCommandPack]) -> String {
    let mut output =
        String::from("pub const BUILTIN_COMMAND_PACKS: &[BuiltinCommandPackDefinition] = &[\n");
    for pack in packs {
        output.push_str("    BuiltinCommandPackDefinition {\n");
        output.push_str(&format!("        id: {},\n", rust_string(&pack.pack.id)));
        output.push_str(&format!(
            "        title: {},\n",
            rust_string(&pack.pack.title)
        ));
        output.push_str(&format!(
            "        owner: {},\n",
            rust_optional_string(pack.pack.owner.as_deref())
        ));
        output.push_str(&format!(
            "        about: {},\n",
            rust_optional_string(pack.pack.about.as_deref())
        ));
        output.push_str("    },\n");
    }
    output.push_str("];\n\n");

    output.push_str("pub const BUILTIN_COMMANDS: &[BuiltinCommandDefinition] = &[\n");
    for pack in packs {
        for command in &pack.commands {
            output.push_str("    BuiltinCommandDefinition {\n");
            output.push_str(&format!(
                "        pack_id: {},\n",
                rust_string(&pack.pack.id)
            ));
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
            output.push_str(&format!(
                "        tags: {},\n",
                rust_string_slice(&command.tags)
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
