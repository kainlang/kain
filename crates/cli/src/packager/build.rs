use std::fs;
use std::path::PathBuf;
use crate::parse_compile_target;
use crate::target_extension;
use crate::error::{KainError, KainResult};
use super::config::PackageManifest;

/// Build all targets specified in KAIN.toml
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

fn build_targets(manifest: &PackageManifest, cwd: &PathBuf, targets: &[String]) -> KainResult<()> {
    use crate::compile;
    
    // Ensure output directory exists
    let output_dir = cwd.join(&manifest.build.output);
    fs::create_dir_all(&output_dir).map_err(|e| KainError::Io(e))?;

    // Read source
    let entry_path = cwd.join(&manifest.build.entry);
    if !entry_path.exists() {
        return Err(KainError::runtime(format!(
            "Entry file not found: {}", entry_path.display()
        )));
    }

    let source = fs::read_to_string(&entry_path).map_err(|e| KainError::Io(e))?;

    for target_str in targets {
        let target = parse_target(target_str)?;
        println!("🎯 Building for target: {:?}", target);

        let output = compile(&source, target)?;
        
        let ext = target_extension(target);
        let output_file = output_dir.join(format!(
            "{}.{}", 
            manifest.package.name, 
            ext
        ));

        fs::write(&output_file, output).map_err(|e| KainError::Io(e))?;
        println!("   ✓ {}", output_file.display());
    }

    Ok(())
}

fn parse_target(s: &str) -> KainResult<kain_core::CompileTarget> {
    parse_compile_target(s).ok_or_else(|| KainError::runtime(format!("Unknown target: {}", s)))
}
