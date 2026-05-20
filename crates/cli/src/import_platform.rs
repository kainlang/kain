use crate::error::{KainError, KainResult};
use kain_c_ffi::{ImportPlatformOptions, PrepareContext};
use std::path::PathBuf;

#[derive(Debug, Clone)]
pub struct ImportPlatformCliOptions {
    pub package_name: Option<String>,
    pub provider: String,
    pub sdk_root: Option<PathBuf>,
    pub output_dir: Option<PathBuf>,
    pub target_triple: Option<String>,
    pub dry_run: bool,
    pub report_json: Option<PathBuf>,
    pub registry_path: Option<PathBuf>,
    pub header_path: Option<PathBuf>,
}

pub fn import_platform(package_or_path: &str, options: ImportPlatformCliOptions) -> KainResult<()> {
    let prepare = PrepareContext {
        current_dir: std::env::current_dir().ok(),
        manifest_path: None,
    };
    let output = kain_c_ffi::import_platform_package(
        package_or_path,
        &ImportPlatformOptions {
            package_name: options.package_name,
            provider: options.provider,
            sdk_root: options.sdk_root,
            output_dir: options.output_dir,
            target_triple: options.target_triple,
            dry_run: options.dry_run,
            report_json: options.report_json,
            registry_path: options.registry_path,
            header_path: options.header_path,
        },
        &prepare,
    )?;

    println!(
        "✓ Platform package '{}' locked for {}",
        output.lock.package_name, output.lock.target_triple
    );
    println!("  provider: {}", output.lock.provider);
    println!("  dispatch: {}", output.lock.dispatch_model);
    println!("  lock: {}", output.lock_path.display());
    if let Some(module_path) = output.generated_module_path {
        println!("  generated module: {}", module_path.display());
    }
    if let Some(report_path) = output.binding_report_path {
        println!("  binding report: {}", report_path.display());
    }
    if !output.lock.blocked_symbols.is_empty() {
        println!(
            "  blocked/unsupported: {}",
            output.lock.blocked_symbols.len()
        );
    }
    if options.dry_run {
        let json = serde_json::to_string_pretty(&output.lock).map_err(|err| {
            KainError::runtime(format!("failed to serialize platform lock: {err}"))
        })?;
        println!("{json}");
    }
    Ok(())
}
