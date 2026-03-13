use crate::error::KainResult;
use std::fs;
use std::path::Path;

#[cfg(feature = "ue5")]
use ue5_materials::{MaterialFactoryGenerator, MaterialGraph};

/// Generate material factory files for runtime material creation.
/// Creates MaterialFactories.h/cpp in `private_dir/Generated/`.
///
/// `private_dir` is the plugin's Source/<Module>/Private directory as
/// computed by PluginLayout — callers should pass `layout.private_dir`.
#[cfg(feature = "ue5")]
pub fn generate_material_factories(
    plugin_name: &str,
    graphs: &[MaterialGraph],
    private_dir: &Path,
) -> KainResult<()> {
    if graphs.is_empty() {
        return Ok(());
    }

    let generator = MaterialFactoryGenerator::new(plugin_name.to_string());

    // Place Generated/ directly under the layout's private directory — this
    // is always correct regardless of whether the plugin uses split-module
    // layout or the legacy flat layout.
    let generated_dir = private_dir.join("Generated");
    fs::create_dir_all(&generated_dir)?;

    // Generate header
    let header = generator.generate_factory_header(graphs);
    fs::write(generated_dir.join("MaterialFactories.h"), header)?;

    // Generate cpp
    let cpp = generator.generate_factory_cpp(graphs);
    fs::write(generated_dir.join("MaterialFactories.cpp"), cpp)?;

    println!(
        "✓ Generated material factories for {} materials",
        graphs.len()
    );

    Ok(())
}

/// Stub implementation when ue5 feature is disabled
#[cfg(not(feature = "ue5"))]
pub fn generate_material_factories(
    _plugin_name: &str,
    _graphs: &[()],
    _private_dir: &Path,
) -> KainResult<()> {
    Ok(())
}
