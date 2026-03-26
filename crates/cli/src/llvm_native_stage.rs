use crate::{
    compile_realtime_app_bundle, compile_runtime_contract_bundle, compile_shader_artifact_bundle,
    CompileTarget,
};
use kain_driver::{write_compute_residency_sidecars, COMPUTE_RESIDENCY_FILE_NAME};
use std::fs;
use std::path::{Path, PathBuf};

pub const SHADER_BUNDLE_FILE_NAME: &str = "kain_shader_bundle.json";
pub const GPU_RUNTIME_WINDOWS_DLL_FILE_NAME: &str = "kain_gpu_runtime.dll";

#[derive(Debug, Clone)]
pub struct LlvmNativeArtifactStage {
    pub runtime_contract_path: PathBuf,
    pub realtime_app_path: PathBuf,
    pub compute_residency_path: Option<PathBuf>,
    pub compute_residency_payload_paths: Vec<PathBuf>,
    pub shader_bundle_path: Option<PathBuf>,
}

pub fn stage_llvm_native_artifacts(
    source: &str,
    output_path: &Path,
    root_component: Option<&str>,
) -> Result<LlvmNativeArtifactStage, String> {
    let contract_bundle = compile_runtime_contract_bundle(source, CompileTarget::Llvm)
        .map_err(|err| err.to_string())?;
    let runtime_contract_path = runtime_contract_artifact_path(output_path);
    write_json_artifact(
        &runtime_contract_path,
        &kain_core::runtime_contract_bundle_to_json(&contract_bundle)
            .map_err(|err| err.to_string())?,
        "runtime contract",
    )?;

    let realtime_bundle = compile_realtime_app_bundle(source, CompileTarget::Llvm, root_component)
        .map_err(|err| err.to_string())?;
    let realtime_app_path = realtime_app_artifact_path(output_path);
    write_json_artifact(
        &realtime_app_path,
        &realtime_bundle.bundle_json,
        "realtime app",
    )?;

    let compute_artifact_paths = write_compute_residency_sidecars(
        &realtime_bundle.bundle,
        output_path.parent().unwrap_or_else(|| Path::new(".")),
    )
    .map_err(|err| err.to_string())?;
    let compute_residency_path = compute_artifact_paths
        .iter()
        .find(|path| {
            path.file_name().and_then(|value| value.to_str()) == Some(COMPUTE_RESIDENCY_FILE_NAME)
        })
        .cloned();
    let compute_residency_payload_paths = compute_artifact_paths
        .into_iter()
        .filter(|path| {
            path.file_name().and_then(|value| value.to_str()) != Some(COMPUTE_RESIDENCY_FILE_NAME)
        })
        .collect::<Vec<_>>();

    let shader_bundle_path = match compile_shader_artifact_bundle(source) {
        Ok(bundle_output) => {
            let shader_path = shader_bundle_artifact_path(output_path);
            write_json_artifact(&shader_path, &bundle_output.bundle_json, "shader bundle")?;
            Some(shader_path)
        }
        Err(err) => {
            let message = err.to_string();
            if message.contains("no entry points")
                || message.contains("expected a shader item")
                || message.contains("SPIR-V backend emitted no entry points")
            {
                None
            } else {
                return Err(message);
            }
        }
    };

    Ok(LlvmNativeArtifactStage {
        runtime_contract_path,
        realtime_app_path,
        compute_residency_path,
        compute_residency_payload_paths,
        shader_bundle_path,
    })
}

pub fn stage_gpu_runtime_dll(executable_path: &Path) -> Result<Option<PathBuf>, String> {
    if !cfg!(windows) {
        return Ok(None);
    }

    let Some(workspace_root) = find_workspace_root_for_gpu_runtime() else {
        return Ok(None);
    };

    let status = std::process::Command::new("cargo")
        .arg("build")
        .arg("-p")
        .arg("kain-gpu-runtime")
        .current_dir(&workspace_root)
        .status()
        .map_err(|err| format!("unable to invoke cargo for kain-gpu-runtime: {err}"))?;
    if !status.success() {
        return Err("cargo build -p kain-gpu-runtime failed".to_string());
    }

    let built_dll = workspace_root
        .join("target")
        .join("debug")
        .join(GPU_RUNTIME_WINDOWS_DLL_FILE_NAME);
    if !built_dll.exists() {
        return Ok(None);
    }

    let destination = executable_path
        .parent()
        .unwrap_or_else(|| Path::new("."))
        .join(GPU_RUNTIME_WINDOWS_DLL_FILE_NAME);
    fs::copy(&built_dll, &destination).map_err(|err| {
        format!(
            "unable to copy kain-gpu-runtime dll {} -> {}: {}",
            built_dll.display(),
            destination.display(),
            err
        )
    })?;
    Ok(Some(destination))
}

pub fn runtime_contract_artifact_path(output_path: &Path) -> PathBuf {
    output_path.with_extension("runtime_contract.json")
}

pub fn realtime_app_artifact_path(output_path: &Path) -> PathBuf {
    output_path.with_extension("realtime_app.json")
}

pub fn shader_bundle_artifact_path(output_path: &Path) -> PathBuf {
    output_path
        .parent()
        .unwrap_or_else(|| Path::new("."))
        .join(SHADER_BUNDLE_FILE_NAME)
}

fn write_json_artifact(path: &Path, contents: &str, label: &str) -> Result<(), String> {
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent).map_err(|err| {
            format!(
                "unable to create {} directory {}: {}",
                label,
                parent.display(),
                err
            )
        })?;
    }
    fs::write(path, contents.as_bytes()).map_err(|err| {
        format!(
            "unable to write {} artifact {}: {}",
            label,
            path.display(),
            err
        )
    })?;
    Ok(())
}

fn find_workspace_root_for_gpu_runtime() -> Option<PathBuf> {
    let mut roots = Vec::new();
    if let Ok(cwd) = std::env::current_dir() {
        roots.push(cwd);
    }
    if let Ok(exe) = std::env::current_exe() {
        if let Some(dir) = exe.parent() {
            roots.push(dir.to_path_buf());
        }
    }
    if let Ok(manifest_dir) = std::env::var("CARGO_MANIFEST_DIR") {
        roots.push(PathBuf::from(manifest_dir));
    }

    for root in roots {
        let mut cursor = root.clone();
        loop {
            if cursor
                .join("crates")
                .join("kain-gpu-runtime")
                .join("Cargo.toml")
                .exists()
            {
                return Some(cursor);
            }
            if !cursor.pop() {
                break;
            }
        }
    }

    None
}

#[cfg(test)]
mod tests {
    use super::{
        runtime_contract_artifact_path, stage_llvm_native_artifacts, SHADER_BUNDLE_FILE_NAME,
    };
    use std::fs;
    use std::path::Path;
    use tempfile::TempDir;

    #[test]
    fn runtime_contract_artifact_path_stays_stable_for_llvm_outputs() {
        let contract_path = runtime_contract_artifact_path(Path::new("build/demo.ll"));
        assert_eq!(contract_path, Path::new("build/demo.runtime_contract.json"));
    }

    #[test]
    fn stage_llvm_native_artifacts_materializes_compute_payloads() {
        let temp = TempDir::new().expect("temp dir");
        let output_path = temp.path().join("build").join("demo.ll");
        let source = r#"
component App():
    render <panel title="LLVM Native" />

shader compute SampleCompute(id: UVec3) -> Vec4:
    uniform src: StorageBuffer<Vec4> @0
    uniform dst: StorageBuffer<Vec4> @1
    return vec4(1.0, 1.0, 1.0, 1.0)
"#;

        let staged = stage_llvm_native_artifacts(source, &output_path, None)
            .expect("llvm native artifacts should stage");

        assert!(staged.runtime_contract_path.exists());
        assert!(staged.realtime_app_path.exists());
        assert!(staged.compute_residency_path.is_some());
        assert!(!staged.compute_residency_payload_paths.is_empty());
        assert!(staged
            .compute_residency_payload_paths
            .iter()
            .all(|path| path.exists()));
        assert_eq!(
            staged
                .shader_bundle_path
                .as_ref()
                .and_then(|path| path.file_name())
                .and_then(|value| value.to_str()),
            Some(SHADER_BUNDLE_FILE_NAME)
        );

        let residency_json = fs::read_to_string(
            staged
                .compute_residency_path
                .as_ref()
                .expect("compute residency path"),
        )
        .expect("residency json");
        assert!(residency_json.contains("SampleCompute"));
    }

    #[test]
    fn stage_llvm_native_artifacts_skips_optional_gpu_sidecars_for_ui_only_source() {
        let temp = TempDir::new().expect("temp dir");
        let output_path = temp.path().join("build").join("demo.ll");
        let source = r#"
component App():
    render <panel title="UI Only" />
"#;

        let staged = stage_llvm_native_artifacts(source, &output_path, None)
            .expect("llvm native artifacts should stage");

        assert!(staged.runtime_contract_path.exists());
        assert!(staged.realtime_app_path.exists());
        assert!(staged.compute_residency_path.is_none());
        assert!(staged.compute_residency_payload_paths.is_empty());
        assert!(staged.shader_bundle_path.is_none());
    }
}
