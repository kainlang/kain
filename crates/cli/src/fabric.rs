use std::path::PathBuf;

use clap::{Subcommand, ValueEnum};

use crate::error::{KainError, KainResult};

#[derive(Subcommand, Debug)]
pub enum FabricCommand {
    Init {
        #[arg(default_value = ".")]
        path: PathBuf,
        #[arg(long, value_enum, default_value_t = FabricTemplateArg::Polyglot)]
        template: FabricTemplateArg,
    },
    Validate {
        #[arg(short, long, default_value = "KAIN.fabric.toml")]
        manifest: PathBuf,
    },
    Run {
        #[arg(short, long, default_value = "KAIN.fabric.toml")]
        manifest: PathBuf,
    },
}

#[derive(Clone, Copy, Debug, ValueEnum)]
pub enum FabricTemplateArg {
    Local,
    Polyglot,
}

impl From<FabricTemplateArg> for kain_omni::FabricTemplateKind {
    fn from(value: FabricTemplateArg) -> Self {
        match value {
            FabricTemplateArg::Local => kain_omni::FabricTemplateKind::Local,
            FabricTemplateArg::Polyglot => kain_omni::FabricTemplateKind::Polyglot,
        }
    }
}

pub fn run(command: FabricCommand) -> KainResult<()> {
    match command {
        FabricCommand::Init { path, template } => {
            let result = kain_omni::init_fabric_manifest(&path, template.into())
                .map_err(|err| KainError::runtime(format!("Fabric init failed: {err}")))?;
            println!(
                "Created Fabric manifest: {}",
                result.manifest_path.display()
            );
            println!("Created paths:");
            for created in &result.created_paths {
                println!("  - {}", created.display());
            }
            Ok(())
        }
        FabricCommand::Validate { manifest } => {
            let result = kain_omni::validate_fabric_manifest_path(&manifest)
                .map_err(|err| KainError::runtime(format!("Fabric validate failed: {err}")))?;
            print_validation_summary(&result);
            Ok(())
        }
        FabricCommand::Run { manifest } => {
            let result = kain_omni::validate_fabric_manifest_path(&manifest)
                .map_err(|err| KainError::runtime(format!("Fabric run failed: {err}")))?;
            print_validation_summary(&result);
            Err(KainError::runtime(
                "Fabric execution is not wired yet. Manifest validated successfully.".to_string(),
            ))
        }
    }
}

fn print_validation_summary(result: &kain_omni::FabricValidationResult) {
    println!("Fabric manifest: {}", result.manifest_path.display());
    println!("Steps: {}", result.step_count);
    println!("Runtimes:");
    for (runtime, count) in &result.runtime_counts {
        println!("  - {runtime}: {count}");
    }
    println!("Required capabilities:");
    for capability in &result.required_capabilities {
        println!("  - {capability}");
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use clap::Parser;

    #[derive(Parser, Debug)]
    struct TestCli {
        #[command(subcommand)]
        command: FabricCommand,
    }

    #[test]
    fn parses_init_with_polyglot_default() {
        let cli = TestCli::parse_from(["kain", "init"]);

        match cli.command {
            FabricCommand::Init { path, template } => {
                assert_eq!(path, PathBuf::from("."));
                assert!(matches!(template, FabricTemplateArg::Polyglot));
            }
            other => panic!("expected init command, got {:?}", other),
        }
    }

    #[test]
    fn run_init_creates_fabric_manifest() {
        let dir = tempfile::tempdir().unwrap();
        run(FabricCommand::Init {
            path: dir.path().to_path_buf(),
            template: FabricTemplateArg::Local,
        })
        .unwrap();

        assert!(dir.path().join("KAIN.fabric.toml").exists());
        assert!(dir.path().join("src").join("main.kn").exists());
    }

    #[test]
    fn run_validate_accepts_generated_manifest() {
        let dir = tempfile::tempdir().unwrap();
        let init =
            kain_omni::init_fabric_manifest(dir.path(), kain_omni::FabricTemplateKind::Polyglot)
                .unwrap();

        run(FabricCommand::Validate {
            manifest: init.manifest_path,
        })
        .unwrap();
    }

    #[test]
    fn run_command_stays_truthful_until_executor_exists() {
        let dir = tempfile::tempdir().unwrap();
        let init =
            kain_omni::init_fabric_manifest(dir.path(), kain_omni::FabricTemplateKind::Local)
                .unwrap();

        let error = run(FabricCommand::Run {
            manifest: init.manifest_path,
        })
        .unwrap_err();
        assert!(error
            .to_string()
            .contains("Fabric execution is not wired yet"));
    }
}
