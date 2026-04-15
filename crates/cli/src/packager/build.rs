use std::fs;
use std::path::{Path, PathBuf};

use crate::error::{KainError, KainResult};
use crate::rust_build;
use crate::{
    compile, compile_hybrid_artifacts, compile_wasm_binary, parse_compile_target, target_extension,
};
use serde::Serialize;

use super::config::PackageManifest;

#[derive(Debug, Serialize)]
struct HybridBundleDescriptor {
    schema_version: u32,
    target: &'static str,
    js: String,
    ts: String,
    wasm: String,
    wasm_exports: Vec<String>,
}

fn patch_hybrid_wasm_reference(source: String, wasm_file_name: &str) -> String {
    let wasm_url_expression = format!(
        "new URL('{wasm_file_name}', document.currentScript?.src ?? window.location.href).toString()"
    );

    source
        .replace("'main.wasm'", &wasm_url_expression)
        .replace("\"main.wasm\"", &wasm_url_expression)
}

fn write_hybrid_bundle(
    descriptor_path: &Path,
    artifacts: crate::HybridArtifactOutput,
) -> KainResult<()> {
    let js_path = descriptor_path.with_extension("js");
    let ts_path = descriptor_path.with_extension("ts");
    let wasm_path = descriptor_path.with_extension("wasm");
    let wasm_file_name = wasm_path
        .file_name()
        .and_then(|value| value.to_str())
        .ok_or_else(|| KainError::runtime(format!("Invalid hybrid wasm path: {}", wasm_path.display())))?;
    let js_file_name = js_path
        .file_name()
        .and_then(|value| value.to_str())
        .ok_or_else(|| KainError::runtime(format!("Invalid hybrid JS path: {}", js_path.display())))?;
    let ts_file_name = ts_path
        .file_name()
        .and_then(|value| value.to_str())
        .ok_or_else(|| KainError::runtime(format!("Invalid hybrid TS path: {}", ts_path.display())))?;
    let descriptor = HybridBundleDescriptor {
        schema_version: 1,
        target: "hybrid",
        js: js_file_name.to_string(),
        ts: ts_file_name.to_string(),
        wasm: wasm_file_name.to_string(),
        wasm_exports: artifacts.wasm_export_names,
    };
    let descriptor_json = serde_json::to_string_pretty(&descriptor).map_err(|err| {
        KainError::runtime(format!("Failed to serialize hybrid bundle descriptor: {err}"))
    })?;

    fs::write(descriptor_path, descriptor_json).map_err(KainError::Io)?;
    fs::write(&js_path, patch_hybrid_wasm_reference(artifacts.js, wasm_file_name))
        .map_err(KainError::Io)?;
    fs::write(&ts_path, patch_hybrid_wasm_reference(artifacts.ts, wasm_file_name))
        .map_err(KainError::Io)?;
    fs::write(&wasm_path, artifacts.wasm).map_err(KainError::Io)?;
    println!("   ✓ {}", descriptor_path.display());
    println!("   ✓ {}", js_path.display());
    println!("   ✓ {}", ts_path.display());
    println!("   ✓ {}", wasm_path.display());
    Ok(())
}

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
        let ext = target_extension(target);
        let output_file = output_dir.join(format!("{}.{}", manifest.package.name, ext));

        match target {
            kain_core::CompileTarget::Wasm => {
                let output = compile_wasm_binary(&source)?;
                fs::write(&output_file, output).map_err(KainError::Io)?;
                println!("   ✓ {}", output_file.display());
            }
            kain_core::CompileTarget::Hybrid => {
                let artifacts = compile_hybrid_artifacts(&source)?;
                write_hybrid_bundle(&output_file, artifacts)?;
            }
            _ => {
                let output = compile(&source, target)?;
                fs::write(&output_file, output).map_err(KainError::Io)?;
                println!("   ✓ {}", output_file.display());
            }
        }
    }

    Ok(())
}

fn parse_target(s: &str) -> KainResult<kain_core::CompileTarget> {
    parse_compile_target(s).ok_or_else(|| KainError::runtime(format!("Unknown target: {}", s)))
}
