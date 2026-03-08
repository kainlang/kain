use std::path::PathBuf;

use clap::Subcommand;

use crate::error::{KainError, KainResult};

#[derive(Subcommand, Debug)]
pub enum OmniCommand {
    Init {
        #[arg(default_value = ".")]
        path: PathBuf,
    },
    Build {
        #[arg(short, long, default_value = "KAIN.omni.toml")]
        manifest: PathBuf,
    },
}

pub fn run(command: OmniCommand) -> KainResult<()> {
    match command {
        OmniCommand::Init { path } => {
            let manifest_path = kain_omni::init_manifest(&path)
                .map_err(|err| KainError::runtime(format!("Omni init failed: {err}")))?;
            println!("Created omni manifest: {}", manifest_path.display());
            Ok(())
        }
        OmniCommand::Build { manifest } => {
            let result = kain_omni::build_manifest_path(&manifest)
                .map_err(|err| KainError::runtime(format!("Omni build failed: {err}")))?;
            println!("Resolved entry: {}", result.resolved_entry.display());
            if !result.staged_imports.is_empty() {
                println!("Staged imports:");
                for staged in &result.staged_imports {
                    println!("  - {} -> {}", staged.source_path.display(), staged.generated_kn_path.display());
                }
            }
            println!("Written outputs:");
            for path in &result.written_outputs {
                println!("  - {}", path.display());
            }
            Ok(())
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use clap::Parser;

    #[derive(Parser, Debug)]
    struct TestCli {
        #[command(subcommand)]
        command: OmniCommand,
    }

    #[test]
    fn parses_init_with_default_path() {
        let cli = TestCli::parse_from(["kain", "init"]);

        match cli.command {
            OmniCommand::Init { path } => assert_eq!(path, PathBuf::from(".")),
            other => panic!("expected init command, got {:?}", other),
        }
    }

    #[test]
    fn parses_build_with_default_manifest() {
        let cli = TestCli::parse_from(["kain", "build"]);

        match cli.command {
            OmniCommand::Build { manifest } => assert_eq!(manifest, PathBuf::from("KAIN.omni.toml")),
            other => panic!("expected build command, got {:?}", other),
        }
    }

    #[test]
    fn run_init_creates_manifest_file() {
        let dir = tempfile::tempdir().unwrap();

        run(OmniCommand::Init {
            path: dir.path().to_path_buf(),
        })
        .unwrap();

        assert!(dir.path().join("KAIN.omni.toml").exists());
    }

    #[test]
    fn run_build_executes_manifest_pipeline() {
        let dir = tempfile::tempdir().unwrap();
        let root = dir.path();

        std::fs::write(
            root.join("main.kn"),
            "fn main() -> Int:\n    return 1\n",
        )
        .unwrap();

        let manifest = kain_omni::OmniManifest {
            project: kain_omni::OmniProject {
                entry: PathBuf::from("main.kn"),
                build_dir: PathBuf::from("omni_out"),
            },
            imports: Vec::new(),
            targets: vec![kain_omni::OmniTarget {
                kind: kain_omni::OmniTargetKind::Rust,
                output: PathBuf::from("omni_out/generated/main"),
                rust_bundle: None,
            }],
            import_resolution: kain_omni::OmniImportResolution::default(),
        };
        let manifest_path = root.join("KAIN.omni.toml");
        std::fs::write(&manifest_path, toml::to_string_pretty(&manifest).unwrap()).unwrap();

        run(OmniCommand::Build {
            manifest: manifest_path,
        })
        .unwrap();

        assert!(root.join("omni_out/generated/main.rs").exists());
    }
}
