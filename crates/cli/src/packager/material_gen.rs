use std::path::Path;
use std::fs;
use crate::error::KainResult;

#[cfg(feature = "ue5")]
use ue5_materials::{MaterialGraph, MaterialFactoryGenerator};

/// Generate material factory files for runtime material creation.
/// Creates MaterialFactories.h/cpp in Generated/ directory.
#[cfg(feature = "ue5")]
pub fn generate_material_factories(
    plugin_name: &str,
    graphs: &[MaterialGraph],
    output_dir: &Path,
) -> KainResult<()> {
    if graphs.is_empty() {
        return Ok(());
    }

    let generator = MaterialFactoryGenerator::new(plugin_name.to_string());
    
    // Create Generated directory
    let generated_dir = output_dir
        .join("Source")
        .join(plugin_name)
        .join("Private")
        .join("Generated");
    fs::create_dir_all(&generated_dir)?;
    
    // Generate header
    let header = generator.generate_factory_header(graphs);
    fs::write(generated_dir.join("MaterialFactories.h"), header)?;
    
    // Generate cpp
    let cpp = generator.generate_factory_cpp(graphs);
    fs::write(generated_dir.join("MaterialFactories.cpp"), cpp)?;
    
    println!("✓ Generated material factories for {} materials", graphs.len());
    
    Ok(())
}

/// Stub implementation when ue5 feature is disabled
#[cfg(not(feature = "ue5"))]
pub fn generate_material_factories(
    _plugin_name: &str,
    _graphs: &[()],  // Empty slice type
    _output_dir: &Path,
) -> KainResult<()> {
    Ok(())
}
