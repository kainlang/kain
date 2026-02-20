pub mod config;
pub mod registry;
pub mod build;
pub mod ue5_pipeline;
pub mod plugin_layout;
pub mod codegen;
pub mod material_gen;
pub mod uplugin_gen;
pub mod build_cs_gen;
pub mod post_process;
pub mod dependencies;
pub mod inject;
pub mod registry_writer;
pub mod cpp_validator;

// Re-export public API to maintain backward compatibility
pub use config::*;
pub use build::build_project;
pub use ue5_pipeline::build_ue5_plugin;
pub use registry::{add_dependency, install_all};
pub use inject::inject_into_plugin;

use std::fs;
use std::path::PathBuf;
use crate::error::{KainError, KainResult};

pub fn init_project(path: &PathBuf, name: Option<String>) -> KainResult<()> {
    if !path.exists() {
        fs::create_dir_all(path).map_err(|e| KainError::Io(e))?;
    }

    let name = name.unwrap_or_else(|| {
        path.file_name()
            .and_then(|s| s.to_str())
            .unwrap_or("my_project")
            .to_string()
    });

    // Create KAIN.toml
    let manifest = PackageManifest::default(&name);
    let toml = toml::to_string_pretty(&manifest)
        .map_err(|e| KainError::runtime(format!("Failed to serialize manifest: {}", e)))?;
    
    fs::write(path.join("KAIN.toml"), toml).map_err(|e| KainError::Io(e))?;

    // Create src directory
    let src_dir = path.join("src");
    fs::create_dir_all(&src_dir).map_err(|e| KainError::Io(e))?;

    // Create main.kn
    let main_src = format!(r#"
# {} - Main Entry Point

fn main():
    println("Hello, KAIN World!")
"#, name);
    
    fs::write(src_dir.join("main.kn"), main_src.trim()).map_err(|e| KainError::Io(e))?;

    // Create .gitignore
    fs::write(path.join(".gitignore"), "target/\ndeps/\n").map_err(|e| KainError::Io(e))?;

    println!(" Initialized new KAIN project: {}", name);
    Ok(())
}

pub fn load_manifest(path: &PathBuf) -> KainResult<PackageManifest> {
    let manifest_path = if path.ends_with("KAIN.toml") {
        path.clone()
    } else {
        path.join("KAIN.toml")
    };

    if !manifest_path.exists() {
        return Err(KainError::runtime(format!(
            "No KAIN.toml found at {}", manifest_path.display()
        )));
    }

    let content = fs::read_to_string(&manifest_path).map_err(|e| KainError::Io(e))?;
    let manifest: PackageManifest = toml::from_str(&content)
        .map_err(|e| KainError::runtime(format!("Failed to parse KAIN.toml: {}", e)))?;
    
    Ok(manifest)
}
