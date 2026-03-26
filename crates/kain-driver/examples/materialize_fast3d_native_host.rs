use std::fs;
use std::path::PathBuf;

use kain_driver::{
    compile_native_app_bundle, materialize_native_app_bundle, NativeAppBundleConfig,
    NativeAppHostSidecarBinding, NativeAppLauncherEntrypoint, NativeAppMaterializationConfig,
    NativeAppRuntimeDependency,
};

struct Fast3dPackagingScenario {
    smoke_directory: &'static str,
    app_slug: &'static str,
    window_title: &'static str,
    packaged_manifest_name: &'static str,
    packaged_host_config_name: &'static str,
    packaged_gameplay_name: Option<&'static str>,
    packaged_shader_overrides_name: Option<&'static str>,
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let scenario_name = std::env::args()
        .nth(1)
        .unwrap_or_else(|| "title-face-snapshot".to_string());
    let scenario = resolve_scenario(&scenario_name)?;

    let workspace_root = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .parent()
        .and_then(|path| path.parent())
        .ok_or("failed to derive workspace root")?
        .to_path_buf();
    let smoke_root = workspace_root
        .join("smoketest")
        .join("3D")
        .join(scenario.smoke_directory);
    let generated_root = smoke_root.join("generated_native_host");
    let project_dir = generated_root.join(scenario.app_slug);
    let executable_output_dir = smoke_root.join("outputs").join("native_host");
    let runtime_crate_dir = workspace_root.join("crates").join("kain-fast3d-runtime");
    let host_config_path = smoke_root
        .join("host_configs")
        .join(scenario.packaged_host_config_name);
    let scene_manifest_path = smoke_root.join(scenario.packaged_manifest_name);

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
    let mut host_sidecars = vec![
        NativeAppHostSidecarBinding {
            source_path: host_config_path,
            packaged_file_name: Some(scenario.packaged_host_config_name.to_string()),
            env_var: Some("KAIN_FAST3D_CONFIG".to_string()),
        },
        NativeAppHostSidecarBinding {
            source_path: scene_manifest_path,
            packaged_file_name: Some(scenario.packaged_manifest_name.to_string()),
            env_var: None,
        },
    ];
    if let Some(gameplay_file_name) = scenario.packaged_gameplay_name {
        host_sidecars.push(NativeAppHostSidecarBinding {
            source_path: smoke_root.join(gameplay_file_name),
            packaged_file_name: Some(gameplay_file_name.to_string()),
            env_var: None,
        });
    }
    if let Some(shader_override_file_name) = scenario.packaged_shader_overrides_name {
        host_sidecars.push(NativeAppHostSidecarBinding {
            source_path: smoke_root.join(shader_override_file_name),
            packaged_file_name: Some(shader_override_file_name.to_string()),
            env_var: None,
        });
    }
    let bundle = compile_native_app_bundle(
        source,
        &NativeAppBundleConfig {
            app_name: Some(scenario.app_slug.to_string()),
            window_title: Some(scenario.window_title.to_string()),
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
            host_sidecars,
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

fn resolve_scenario(
    scenario_name: &str,
) -> Result<Fast3dPackagingScenario, Box<dyn std::error::Error>> {
    let scenario = match scenario_name {
        "title-face-snapshot" => Fast3dPackagingScenario {
            smoke_directory: "sm64_fast3d_smoke",
            app_slug: "sm64-fast3d-native-host-snapshot",
            window_title: "SM64 Fast3D Native Host (title-face snapshot)",
            packaged_manifest_name: "scene_manifest_title_face.json",
            packaged_host_config_name: "title_face_native_host_snapshot.json",
            packaged_gameplay_name: None,
            packaged_shader_overrides_name: None,
        },
        "title-face-viewer" => Fast3dPackagingScenario {
            smoke_directory: "sm64_fast3d_smoke",
            app_slug: "sm64-fast3d-native-host-viewer",
            window_title: "SM64 Fast3D Native Host (title-face viewer)",
            packaged_manifest_name: "scene_manifest_title_face.json",
            packaged_host_config_name: "title_face_native_host_viewer.json",
            packaged_gameplay_name: None,
            packaged_shader_overrides_name: None,
        },
        "bob-snapshot" => Fast3dPackagingScenario {
            smoke_directory: "sm64_bob_level_chunk",
            app_slug: "sm64-bob-native-host-snapshot",
            window_title: "SM64 BOB Native Host (snapshot)",
            packaged_manifest_name: "scene_manifest.json",
            packaged_host_config_name: "native_host_snapshot.json",
            packaged_gameplay_name: Some("gameplay_state.json"),
            packaged_shader_overrides_name: Some("shader_overrides.json"),
        },
        "bob-viewer" => Fast3dPackagingScenario {
            smoke_directory: "sm64_bob_level_chunk",
            app_slug: "sm64-bob-native-host-viewer",
            window_title: "SM64 BOB Native Host (viewer)",
            packaged_manifest_name: "scene_manifest.json",
            packaged_host_config_name: "native_host_viewer.json",
            packaged_gameplay_name: Some("gameplay_state.json"),
            packaged_shader_overrides_name: Some("shader_overrides.json"),
        },
        other => {
            return Err(format!(
                "unsupported scenario '{other}', expected one of: title-face-snapshot, title-face-viewer, bob-snapshot, bob-viewer"
            )
            .into())
        }
    };
    Ok(scenario)
}
