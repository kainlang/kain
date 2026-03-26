use std::fs;
use std::path::PathBuf;

use kain_driver::{
    compile_native_app_bundle, materialize_native_app_bundle, NativeAppBundleConfig,
    NativeAppHostSidecarBinding, NativeAppLauncherEntrypoint, NativeAppMaterializationConfig,
    NativeAppRuntimeDependency,
};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let mode = std::env::args()
        .nth(1)
        .unwrap_or_else(|| "snapshot".to_string());
    let config_file_name = match mode.as_str() {
        "snapshot" => "title_face_native_host_snapshot.json",
        "viewer" => "title_face_native_host_viewer.json",
        other => {
            return Err(
                format!("unsupported mode '{other}', expected 'snapshot' or 'viewer'").into(),
            )
        }
    };

    let workspace_root = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .parent()
        .and_then(|path| path.parent())
        .ok_or("failed to derive workspace root")?
        .to_path_buf();
    let smoke_root = workspace_root
        .join("smoketest")
        .join("3D")
        .join("sm64_fast3d_smoke");
    let generated_root = smoke_root.join("generated_native_host");
    let project_dir = generated_root.join(format!("sm64_fast3d_native_host_{mode}"));
    let executable_output_dir = smoke_root.join("outputs").join("native_host");
    let runtime_crate_dir = workspace_root.join("crates").join("kain-fast3d-runtime");
    let host_config_path = smoke_root.join("host_configs").join(config_file_name);
    let scene_manifest_path = smoke_root.join("scene_manifest_title_face.json");

    if !host_config_path.exists() {
        return Err(format!(
            "expected Fast3D host config at {}",
            host_config_path.display()
        )
        .into());
    }
    if !scene_manifest_path.exists() {
        return Err(format!(
            "expected Fast3D scene manifest at {}",
            scene_manifest_path.display()
        )
        .into());
    }

    fs::create_dir_all(&generated_root)?;
    fs::create_dir_all(&executable_output_dir)?;

    let source = r#"
component App():
    render <panel title="SM64 Fast3D Native Host" />
"#;
    let bundle = compile_native_app_bundle(
        source,
        &NativeAppBundleConfig {
            app_name: Some(format!("sm64-fast3d-native-host-{mode}")),
            window_title: Some(format!("SM64 Fast3D Native Host ({mode})")),
            source_file_name: Some("sm64_fast3d_native_host.kn".to_string()),
            ..Default::default()
        },
    )?;

    let generated = materialize_native_app_bundle(
        source,
        &bundle,
        &NativeAppMaterializationConfig {
            project_dir,
            runtime_crate_name: "kain-fast3d-runtime".to_string(),
            runtime_dependency: NativeAppRuntimeDependency::Path(runtime_crate_dir),
            artifact_output_dir: PathBuf::from("generated"),
            build_executable: true,
            release: false,
            executable_output_dir: Some(executable_output_dir.clone()),
            launcher_entrypoint: NativeAppLauncherEntrypoint::RunNoArgFunction {
                function_name: "run_fast3d_cli".to_string(),
            },
            host_sidecars: vec![
                NativeAppHostSidecarBinding {
                    source_path: host_config_path,
                    packaged_file_name: Some(config_file_name.to_string()),
                    env_var: Some("KAIN_FAST3D_CONFIG".to_string()),
                },
                NativeAppHostSidecarBinding {
                    source_path: scene_manifest_path,
                    packaged_file_name: Some("scene_manifest_title_face.json".to_string()),
                    env_var: None,
                },
            ],
        },
    )?;

    println!(
        "Generated Fast3D native host project at {}",
        generated.project_dir.display()
    );
    if let Some(executable_path) = generated.executable_path {
        println!("Executable: {}", executable_path.display());
    }
    println!("Executable output dir: {}", executable_output_dir.display());
    Ok(())
}
