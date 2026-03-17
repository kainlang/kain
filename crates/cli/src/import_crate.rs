use crate::error::KainResult;
use kain_crate_ffi::{ImportCrateOptions, PrepareContext};

pub fn import_crate(crate_name: &str, options: ImportCrateOptions) -> KainResult<()> {
    let prepare = PrepareContext {
        current_dir: std::env::current_dir().ok(),
        manifest_path: options.manifest_path.clone(),
    };
    let output = kain_crate_ffi::import_crate(crate_name, &options, &prepare)?;

    println!(
        "✓ Imported Rust crate '{}' ({:?})",
        output.resolved.import_name, output.resolved.resolution_kind
    );
    println!("  Cache: {}", output.cache_dir.display());
    println!("  Module: {}", output.canonical_module_path.display());
    println!("  Prelude: {}", output.prelude_path.display());
    println!("  Report JSON: {}", output.report_json_path.display());
    println!("  Report Text: {}", output.report_text_path.display());
    println!("  Bridge Manifest: {}", output.bridge_manifest_path.display());
    if let Some(dylib_path) = output.dylib_path.as_ref() {
        println!("  Bridge Library: {}", dylib_path.display());
    }
    println!(
        "  Live Bridge: {}",
        if options.mode.wants_live() {
            "enabled"
        } else {
            "skipped"
        }
    );
    println!(
        "  Cache Reused: {}",
        if output.cache_hit { "yes" } else { "no" }
    );

    Ok(())
}
