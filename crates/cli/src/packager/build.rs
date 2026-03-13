use std::fs;
use std::path::PathBuf;

use crate::error::{KainError, KainResult};
use crate::rust_build;
use crate::{compile, parse_compile_target, target_extension};

use super::config::PackageManifest;

pub fn build_project(target_overrides: Option<Vec<String>>) -> KainResult<()> {
    let cwd = std::env::current_dir().unwrap_or_else(|_| PathBuf::from("."));
    let manifest = super::load_manifest(&cwd)?;

    let targets = if let Some(ref overrides) = target_overrides {
        overrides.clone()
    } else {
        manifest.build.targets.clone()
    };

    if targets.is_empty() {
        return Err(KainError::runtime("No build targets specified"));
    }

    build_targets(&manifest, &cwd, &targets)
}

pub fn build_rust_project() -> KainResult<()> {
    let cwd = std::env::current_dir().unwrap_or_else(|_| PathBuf::from("."));
    let manifest = super::load_manifest(&cwd)?;
    let config = manifest.r#rust.clone().unwrap_or_default();
    let entry_path = cwd.join(&manifest.build.entry);
    if !entry_path.exists() {
        return Err(KainError::runtime(format!(
            "Entry file not found: {}",
            entry_path.display()
        )));
    }

    let output_root = config
        .output
        .clone()
        .map(|path| {
            if path.is_absolute() {
                path
            } else {
                cwd.join(path)
            }
        })
        .unwrap_or_else(|| cwd.join(&manifest.build.output).join("rust"));

    let written =
        rust_build::run_rust_build_pipeline(&entry_path, Some(&output_root), Some(&config))?;

    for path in written {
        println!("   ✓ {}", path.display());
    }

    Ok(())
}

fn build_targets(manifest: &PackageManifest, cwd: &PathBuf, targets: &[String]) -> KainResult<()> {
    let output_dir = cwd.join(&manifest.build.output);
    fs::create_dir_all(&output_dir).map_err(KainError::Io)?;

    let entry_path = cwd.join(&manifest.build.entry);
    if !entry_path.exists() {
        return Err(KainError::runtime(format!(
            "Entry file not found: {}",
            entry_path.display()
        )));
    }

    let source = fs::read_to_string(&entry_path).map_err(KainError::Io)?;

    for target_str in targets {
        let target = parse_target(target_str)?;
        println!("🎯 Building for target: {:?}", target);

        let output = compile(&source, target)?;
        let ext = target_extension(target);
        let output_file = output_dir.join(format!("{}.{}", manifest.package.name, ext));

        fs::write(&output_file, output).map_err(KainError::Io)?;
        println!("   ✓ {}", output_file.display());
    }

    Ok(())
}

fn parse_target(s: &str) -> KainResult<kain_core::CompileTarget> {
    parse_compile_target(s).ok_or_else(|| KainError::runtime(format!("Unknown target: {}", s)))
}
