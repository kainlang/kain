use std::fs;
use std::path::PathBuf;
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
    use crate::{compile, CompileTarget};
    
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
    use kain_core::CompileTarget;
    match s.to_lowercase().as_str() {
        "wasm" | "w" => Ok(CompileTarget::Wasm),
        "js" | "javascript" | "j" => Ok(CompileTarget::Js),
        "cpp" | "c++" => Ok(CompileTarget::Cpp),
        "rust" | "rs" => Ok(CompileTarget::Rust),
        "ue5" | "unreal" | "u" => Ok(CompileTarget::Ue5),
        "ue5-editor" | "editor" => Ok(CompileTarget::Ue5Editor),
        "usf" | "shader" => Ok(CompileTarget::Usf),
        "spirv" | "spv" => Ok(CompileTarget::Spirv),
        "hlsl" => Ok(CompileTarget::Hlsl),
        "hybrid" => Ok(CompileTarget::Hybrid),
        "llvm" => Ok(CompileTarget::Llvm),
        "interpret" | "run" | "i" | "r" => Ok(CompileTarget::Interpret),
        "test" | "t" => Ok(CompileTarget::Test),
        _ => Err(KainError::runtime(format!("Unknown target: {}", s))),
    }
}

fn target_extension(target: kain_core::CompileTarget) -> &'static str {
    use kain_core::CompileTarget;
    match target {
        CompileTarget::Wasm => "wasm",
        CompileTarget::Js => "js",
        CompileTarget::Cpp => "cpp",
        CompileTarget::Rust => "rs",
        CompileTarget::Ue5 | CompileTarget::Ue5Editor => "h",
        CompileTarget::Usf | CompileTarget::Hlsl => "hlsl",
        CompileTarget::Spirv => "spv",
        CompileTarget::Hybrid => "hybrid",
        CompileTarget::Llvm => "ll",
        CompileTarget::Interpret | CompileTarget::Test => "txt",
    }
}
