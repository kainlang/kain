use std::fs;
use std::path::PathBuf;
use flate2::read::GzDecoder;
use tar::Archive;
use crate::error::{KainError, KainResult};
use super::config::{self, PackageMeta, RegistryIndex};

pub fn add_dependency(package_name: &str, version: Option<String>) -> KainResult<()> {
    println!(" Fetching registry index...");
    let index: RegistryIndex = reqwest::blocking::get(config::registry_url())
        .map_err(|e| KainError::runtime(format!("Failed to fetch registry: {}", e)))?
        .json()
        .map_err(|e| KainError::runtime(format!("Failed to parse registry: {}", e)))?;

    let meta_url = index.packages.get(package_name)
        .ok_or_else(|| KainError::runtime(format!("Package '{}' not found in registry", package_name)))?;

    let pkg_meta: PackageMeta = reqwest::blocking::get(meta_url)
        .map_err(|e| KainError::runtime(format!("Failed to fetch package metadata: {}", e)))?
        .json()
        .map_err(|e| KainError::runtime(format!("Failed to parse package metadata: {}", e)))?;

    let version_to_install = version.unwrap_or_else(|| {
        pkg_meta.versions.keys()
            .max()
            .cloned()
            .unwrap_or_else(|| "0.1.0".to_string())
    });

    let pkg_ver = pkg_meta.versions.get(&version_to_install)
        .ok_or_else(|| KainError::runtime(format!("Version {} not found", version_to_install)))?;

    // Add to KAIN.toml
    let cwd = std::env::current_dir().unwrap_or_else(|_| PathBuf::from("."));
    let mut manifest = super::load_manifest(&cwd)?;
    manifest.dependencies.insert(
        package_name.to_string(),
        version_to_install.clone()
    );
    
    let toml = toml::to_string_pretty(&manifest)
        .map_err(|e| KainError::runtime(format!("Failed to serialize: {}", e)))?;
    fs::write(cwd.join("KAIN.toml"), toml).map_err(|e| KainError::Io(e))?;

    println!("   Added {} v{}", package_name, version_to_install);

    // Install
    install_package(package_name, &version_to_install, &pkg_ver.url)
}

pub fn install_all() -> KainResult<()> {
    let cwd = std::env::current_dir().unwrap_or_else(|_| PathBuf::from("."));
    let manifest = super::load_manifest(&cwd)?;

    if manifest.dependencies.is_empty() {
        println!(" No dependencies to install.");
        return Ok(());
    }

    println!("📦 Installing {} dependencies...", manifest.dependencies.len());

    let index: RegistryIndex = reqwest::blocking::get(config::registry_url())
        .map_err(|e| KainError::runtime(format!("Failed to fetch registry: {}", e)))?
        .json()
        .map_err(|e| KainError::runtime(format!("Failed to parse registry: {}", e)))?;

    for (name, version) in &manifest.dependencies {
        if let Some(meta_url) = index.packages.get(name) {
            let pkg_meta: PackageMeta = reqwest::blocking::get(meta_url)
                .map_err(|e| KainError::runtime(format!("Failed to fetch {}: {}", name, e)))?
                .json()
                .map_err(|e| KainError::runtime(format!("Failed to parse {}: {}", name, e)))?;

            if let Some(pkg_ver) = pkg_meta.versions.get(version) {
                install_package(name, version, &pkg_ver.url)?;
            } else {
                eprintln!("⚠️  Version {} not found for {}", version, name);
            }
        } else {
            eprintln!("⚠️  Package {} not found in registry", name);
        }
    }

    Ok(())
}

fn install_package(name: &str, version: &str, url: &str) -> KainResult<()> {
    let deps_dir = PathBuf::from("deps");
    if !deps_dir.exists() {
        fs::create_dir_all(&deps_dir).map_err(|e| KainError::Io(e))?;
    }

    let pkg_dir = deps_dir.join(name);
    if pkg_dir.exists() {
        println!("   {} v{} already installed, updating...", name, version);
        fs::remove_dir_all(&pkg_dir).map_err(|e| KainError::Io(e))?;
    }

    println!("📥 Downloading {} v{}...", name, version);
    
    let response = reqwest::blocking::get(url)
        .map_err(|e| KainError::runtime(format!("Download failed: {}", e)))?;
    
    let bytes = response.bytes()
        .map_err(|e| KainError::runtime(format!("Failed to read response: {}", e)))?;
    
    // Extract tarball
    let decoder = GzDecoder::new(&bytes[..]);
    let mut archive = Archive::new(decoder);
    
    fs::create_dir_all(&pkg_dir).map_err(|e| KainError::Io(e))?;
    archive.unpack(&pkg_dir)
        .map_err(|e| KainError::runtime(format!("Failed to extract: {}", e)))?;

    println!("   ✓ Installed {} v{}", name, version);
    Ok(())
}
