use kain_core::ast::{Item, Program};
use kain_core::diagnostics::SpanMapper;
use kain_core::format_program;
use kain_core::lexer::Lexer;
use kain_core::parser::Parser;
use kain_core::tooling_config::apply_cargo_command_defaults;
use kain_core::CompileTarget;
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

impl LlvmNativeArtifactStage {
    pub fn requires_gpu_runtime_dll(&self) -> bool {
        self.compute_residency_path.is_some() || !self.compute_residency_payload_paths.is_empty()
    }
}

pub fn stage_llvm_native_artifacts(
    source: &str,
    output_path: &Path,
    root_component: Option<&str>,
) -> Result<LlvmNativeArtifactStage, String> {
    stage_native_backend_artifacts(source, CompileTarget::Llvm, output_path, root_component)
}

pub fn stage_native_backend_artifacts(
    source: &str,
    target: CompileTarget,
    output_path: &Path,
    root_component: Option<&str>,
) -> Result<LlvmNativeArtifactStage, String> {
    let session = kain_driver::DriverSession::default();
    stage_native_backend_artifacts_with_session(
        &session,
        source,
        None,
        target,
        output_path,
        root_component,
    )
}

pub fn stage_native_backend_artifacts_with_session(
    session: &kain_driver::DriverSession,
    source: &str,
    source_path: Option<&Path>,
    target: CompileTarget,
    output_path: &Path,
    root_component: Option<&str>,
) -> Result<LlvmNativeArtifactStage, String> {
    let contract_bundle = session
        .compile_runtime_contract_bundle_with_source_path(source, source_path, target)
        .map_err(|err| err.to_string())?;
    let runtime_contract_path = runtime_contract_artifact_path(output_path);
    write_json_artifact(
        &runtime_contract_path,
        &kain_core::runtime_contract_bundle_to_json(&contract_bundle)
            .map_err(|err| err.to_string())?,
        "runtime contract",
    )?;

    let realtime_bundle = session
        .compile_realtime_app_bundle_with_source_path(source, source_path, target, root_component)
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

    let shader_bundle_path = if source_declares_shader_item(source) {
        let extracted_shader_source;
        let shader_source = match shader_artifact_source(source) {
            Some(source) => {
                extracted_shader_source = source;
                extracted_shader_source.as_str()
            }
            None => source,
        };

        match session.compile_shader_artifact_bundle(shader_source) {
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
        }
    } else {
        None
    };

    Ok(LlvmNativeArtifactStage {
        runtime_contract_path,
        realtime_app_path,
        compute_residency_path,
        compute_residency_payload_paths,
        shader_bundle_path,
    })
}

fn source_declares_shader_item(source: &str) -> bool {
    let Ok(tokens) = Lexer::new(source).tokenize() else {
        return true;
    };
    let mapper = SpanMapper::new(source);
    let Ok(program) = Parser::new(&tokens, &mapper, "<native-backend-shader-scan>").parse() else {
        return true;
    };
    program.items.iter().any(item_declares_shader_item)
}

fn item_declares_shader_item(item: &Item) -> bool {
    match item {
        Item::Shader(_) => true,
        Item::Mod(module) => module
            .inline
            .as_ref()
            .map(|items| items.iter().any(item_declares_shader_item))
            .unwrap_or(false),
        _ => false,
    }
}

fn shader_artifact_source(source: &str) -> Option<String> {
    let tokens = Lexer::new(source).tokenize().ok()?;
    let mapper = SpanMapper::new(source);
    let program = Parser::new(&tokens, &mapper, "<native-backend-shader-extract>")
        .parse()
        .ok()?;
    let shader_items = filter_shader_items(&program.items);
    if shader_items.is_empty() {
        return None;
    }

    let shader_program = Program {
        items: shader_items,
        span: program.span,
    };
    format_program(&shader_program).ok()
}

fn filter_shader_items(items: &[Item]) -> Vec<Item> {
    items
        .iter()
        .filter_map(filter_shader_item)
        .collect::<Vec<_>>()
}

fn filter_shader_item(item: &Item) -> Option<Item> {
    match item {
        Item::Shader(shader) => Some(Item::Shader(shader.clone())),
        Item::Mod(module) => {
            let inline = module.inline.as_ref()?;
            let filtered_inline = filter_shader_items(inline);
            if filtered_inline.is_empty() {
                return None;
            }

            let mut filtered_module = module.clone();
            filtered_module.inline = Some(filtered_inline);
            Some(Item::Mod(filtered_module))
        }
        _ => None,
    }
}

pub fn stage_gpu_runtime_dll(executable_path: &Path) -> Result<Option<PathBuf>, String> {
    if !cfg!(windows) {
        return Ok(None);
    }

    let Some(workspace_root) = find_workspace_root_for_gpu_runtime() else {
        return Ok(None);
    };
    let cargo_target_dir = executable_path
        .parent()
        .unwrap_or_else(|| Path::new("."))
        .join(".kain")
        .join("cargo-target")
        .join("gpu-runtime");
    fs::create_dir_all(&cargo_target_dir).map_err(|err| {
        format!(
            "unable to create kain-gpu-runtime cargo target dir {}: {}",
            cargo_target_dir.display(),
            err
        )
    })?;

    let mut command = std::process::Command::new("cargo");
    command
        .arg("build")
        .arg("-p")
        .arg("kain-gpu-runtime")
        .env("CARGO_TARGET_DIR", &cargo_target_dir)
        .current_dir(&workspace_root);
    apply_cargo_command_defaults(&mut command);
    let status = command
        .status()
        .map_err(|err| format!("unable to invoke cargo for kain-gpu-runtime: {err}"))?;
    if !status.success() {
        return Err("cargo build -p kain-gpu-runtime failed".to_string());
    }

    let built_dll = cargo_target_dir
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
        assert!(staged.requires_gpu_runtime_dll());
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
    fn shader_artifact_source_extracts_shaders_from_native_stdlib_source() {
        let source = r#"
shader fragment SampleFragment(uv: Vec2) -> Vec4:
    uniform accent: Vec3 @0
    return vec4(1.0, 1.0, 1.0, 1.0)

fn main() -> Int:
    let status = native_runtime_init()
    let entanglements = native_entangle_registered_count()
    let _shutdown = native_runtime_shutdown()
    return status + entanglements
"#;

        let extracted = super::shader_artifact_source(source)
            .expect("mixed native and shader source should have extracted shader source");
        let bundle = crate::compile_shader_artifact_bundle(&extracted)
            .expect("extracted shader-only source should compile to a shader bundle");

        assert!(extracted.contains("shader fragment SampleFragment"));
        assert!(!extracted.contains("native_runtime_init"));
        assert!(bundle.bundle_json.contains("SampleFragment"));
    }

    #[test]
    fn shader_artifact_source_extracts_kain_example_shaders_without_native_body() {
        let source = include_str!("../../../blades/kain-example/src/main.kn");

        let extracted = super::shader_artifact_source(source)
            .expect("kain-example native source should yield shader-only source");

        assert!(extracted.contains("shader fragment NativeExampleGradient"));
        assert!(extracted.contains("shader compute NativeExampleBlendKernel"));
        assert!(!extracted.contains("native_runtime_heap_validate"));
        assert!(!extracted.contains("fn main()"));

        let bundle = crate::compile_shader_artifact_bundle(&extracted)
            .expect("kain-example extracted shader source should compile to a shader bundle");
        assert!(bundle.bundle_json.contains("NativeExampleGradient"));
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
        assert!(!staged.requires_gpu_runtime_dll());
        assert!(staged.compute_residency_path.is_none());
        assert!(staged.compute_residency_payload_paths.is_empty());
        assert!(staged.shader_bundle_path.is_none());
    }

    #[test]
    fn stage_llvm_native_artifacts_skips_shader_compilation_for_native_stdlib_only_source() {
        let temp = TempDir::new().expect("temp dir");
        let output_path = temp.path().join("build").join("demo.ll");
        let source = r#"
fn main() -> Int:
    let status = native_runtime_init()
    native_runtime_shutdown()
    return status
"#;

        let staged = stage_llvm_native_artifacts(source, &output_path, None)
            .expect("llvm native artifacts should stage");

        assert!(staged.runtime_contract_path.exists());
        assert!(staged.realtime_app_path.exists());
        assert!(!staged.requires_gpu_runtime_dll());
        assert!(staged.compute_residency_path.is_none());
        assert!(staged.compute_residency_payload_paths.is_empty());
        assert!(staged.shader_bundle_path.is_none());
    }

    #[test]
    fn stage_llvm_native_artifacts_materializes_entangle_metadata() {
        let temp = TempDir::new().expect("temp dir");
        let output_path = temp.path().join("build").join("demo.ll");
        let source = r#"
world Physics:
    state player_health: Int = 100
    surface native_ui => App

world UI:
    state health_display: Int = 100
    surface web => App

component App():
    render <panel />

entangle Physics.player_health <-> UI.health_display with single_writer
"#;

        let staged = stage_llvm_native_artifacts(source, &output_path, None)
            .expect("llvm native artifacts should stage");

        let contract_json = fs::read_to_string(&staged.runtime_contract_path)
            .expect("runtime contract json should exist");
        let realtime_json =
            fs::read_to_string(&staged.realtime_app_path).expect("realtime app json should exist");

        for json in [&contract_json, &realtime_json] {
            assert!(json.contains("\"entanglements\""));
            assert!(json.contains("Physics.player_health"));
            assert!(json.contains("UI.health_display"));
            assert!(json.contains("single_writer"));
            assert!(json.contains("state.entangle"));
        }
    }

    #[test]
    fn stage_llvm_native_artifacts_materializes_full_native_intent_metadata() {
        let temp = TempDir::new().expect("temp dir");
        let output_path = temp
            .path()
            .join("build")
            .join("native_world_actor_intent.ll");
        let source = r#"
world Physics:
    state player_health: Int = 100
    surface native_ui => App

world UI:
    state health_display: Int = 100
    surface web => App

component App():
    render <panel />

entangle Physics.player_health <-> UI.health_display with single_writer

patch set_health(physics: Physics, value: Int) -> Int:
    physics.player_health = value
    return physics.player_health

law health_valid(value: Int) -> Bool:
    return value >= 0

converge choose_value(value: Int) -> Int:
    spec reference:
        return value + 1
    fast interpret_lane when target("interpret"):
        return value + 1

fn stage_bias(value: Int) -> Int:
    return value + 2

orchestrate pipeline(value: Int) -> Int:
    let staged: Int = kain choose_value(value)
    let echoed: Int = rust stage_bias(staged)
    return echoed
"#;

        let staged = stage_llvm_native_artifacts(source, &output_path, None)
            .expect("llvm native artifacts should stage");

        let contract_json = fs::read_to_string(&staged.runtime_contract_path)
            .expect("runtime contract json should exist");
        let realtime_json =
            fs::read_to_string(&staged.realtime_app_path).expect("realtime app json should exist");

        for json in [&contract_json, &realtime_json] {
            assert!(json.contains("\"worlds\""));
            assert!(json.contains("\"patches\""));
            assert!(json.contains("\"laws\""));
            assert!(json.contains("\"converges\""));
            assert!(json.contains("\"orchestrations\""));
            assert!(json.contains("\"entanglements\""));
            assert!(json.contains("Physics.player_health"));
            assert!(json.contains("UI.health_display"));
            assert!(json.contains("set_health"));
            assert!(json.contains("health_valid"));
            assert!(json.contains("choose_value"));
            assert!(json.contains("pipeline"));
        }
    }
}
