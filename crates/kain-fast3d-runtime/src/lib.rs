pub mod combiner;
pub mod config;
pub mod extractor;
pub mod math;
pub mod model;
pub mod rasterizer;
pub mod runtime;
pub mod texture;
pub mod viewer;

use std::{
    env,
    path::{Path, PathBuf},
};

pub use config::{load_host_config, Fast3dHostAction, Fast3dHostConfig, ResolvedFast3dHostAction};
pub use extractor::{extract_sm64_level_chunk_scene, extract_sm64_title_face_scene};
pub use runtime::{
    load_gameplay_state_document, load_shader_override_document, Fast3dRuntime,
    GameplayStateDocument, RuntimeFrameBindings,
};
pub use viewer::{launch_viewer, write_snapshot_png, OrbitControls};

pub const KAIN_FAST3D_CONFIG_ENV: &str = "KAIN_FAST3D_CONFIG";
pub const KAIN_FAST3D_MANIFEST_ENV: &str = "KAIN_FAST3D_MANIFEST";
pub const KAIN_FAST3D_DEFAULT_MANIFEST: &str = "scene_manifest.json";
pub const KAIN_FAST3D_DEFAULT_TITLE_FACE_MANIFEST: &str = "scene_manifest_title_face.json";

pub fn run_fast3d_cli() -> Result<(), Box<dyn std::error::Error>> {
    let mut args = env::args().skip(1);
    let Some(first_argument) = args.next() else {
        if let Some(config_path) = env::var_os(KAIN_FAST3D_CONFIG_ENV).map(PathBuf::from) {
            return execute_host_config_path(&config_path);
        }
        let manifest_path = resolve_default_manifest_path();
        let runtime = Fast3dRuntime::load_from_path(&manifest_path)?;
        let runtime_bindings = RuntimeFrameBindings::default();
        launch_viewer(manifest_path, runtime, runtime_bindings)?;
        return Ok(());
    };

    if first_argument == "--config" {
        let config_path = args
            .next()
            .map(PathBuf::from)
            .ok_or("expected path after --config")?;
        if let Some(argument) = args.next() {
            return Err(format!("unrecognized argument `{argument}` after --config").into());
        }
        return execute_host_config_path(&config_path);
    }

    if first_argument == "--extract-sm64-title-face" {
        let sm64_root = args
            .next()
            .map(PathBuf::from)
            .ok_or("expected SM64 source root after --extract-sm64-title-face")?;
        let mut manifest_out = PathBuf::from(KAIN_FAST3D_DEFAULT_TITLE_FACE_MANIFEST);
        while let Some(argument) = args.next() {
            match argument.as_str() {
                "--manifest-out" => {
                    let value = args.next().ok_or("expected path after --manifest-out")?;
                    manifest_out = PathBuf::from(value);
                }
                other => return Err(format!("unrecognized extractor argument `{other}`").into()),
            }
        }
        extract_sm64_title_face_scene(&sm64_root, &manifest_out)?;
        println!(
            "Wrote extracted SM64 title-face scene manifest to {}",
            manifest_out.display()
        );
        return Ok(());
    }

    if first_argument == "--extract-sm64-level-chunk" {
        let sm64_root = args
            .next()
            .map(PathBuf::from)
            .ok_or("expected SM64 source root after --extract-sm64-level-chunk")?;
        let mut level_name = "bob".to_string();
        let mut area_id = 1u32;
        let mut manifest_out = PathBuf::from("scene_manifest_level_chunk.json");
        while let Some(argument) = args.next() {
            match argument.as_str() {
                "--level" => {
                    level_name = args.next().ok_or("expected level name after --level")?;
                }
                "--area" => {
                    let value = args.next().ok_or("expected area id after --area")?;
                    area_id = value.parse::<u32>().map_err(|_| "area id must be a positive integer")?;
                }
                "--manifest-out" => {
                    let value = args.next().ok_or("expected path after --manifest-out")?;
                    manifest_out = PathBuf::from(value);
                }
                other => return Err(format!("unrecognized level-chunk extractor argument `{other}`").into()),
            }
        }
        extract_sm64_level_chunk_scene(&sm64_root, &level_name, area_id, &manifest_out)?;
        println!(
            "Wrote extracted SM64 {} area {} level chunk manifest to {}",
            level_name, area_id, manifest_out.display()
        );
        return Ok(());
    }

    let manifest_path = PathBuf::from(first_argument);
    let mut snapshot_path = None;
    let mut snapshot_time_seconds = 0.0_f32;
    while let Some(argument) = args.next() {
        match argument.as_str() {
            "--snapshot" => {
                let value = args.next().ok_or("expected path after --snapshot")?;
                snapshot_path = Some(PathBuf::from(value));
            }
            "--time-seconds" => {
                let value = args.next().ok_or("expected value after --time-seconds")?;
                snapshot_time_seconds = value.parse::<f32>()?;
            }
            other => return Err(format!("unrecognized argument `{other}`").into()),
        }
    }

    let runtime = Fast3dRuntime::load_from_path(&manifest_path)?;
    let runtime_bindings = RuntimeFrameBindings::default();
    if let Some(snapshot_path) = snapshot_path {
        let frame = runtime.render_frame(
            snapshot_time_seconds,
            &runtime.default_camera_controls(),
            Some(&runtime_bindings),
            None,
        )?;
        write_snapshot_png(&snapshot_path, &frame)?;
        println!(
            "Wrote Fast3D snapshot to {} ({}x{})",
            snapshot_path.display(),
            frame.width,
            frame.height
        );
        return Ok(());
    }

    launch_viewer(manifest_path, runtime, runtime_bindings)?;
    Ok(())
}

fn resolve_default_manifest_path() -> PathBuf {
    env::var_os(KAIN_FAST3D_MANIFEST_ENV)
        .map(PathBuf::from)
        .unwrap_or_else(|| PathBuf::from(KAIN_FAST3D_DEFAULT_MANIFEST))
}

pub fn execute_host_config_path(config_path: &Path) -> Result<(), Box<dyn std::error::Error>> {
    let config = load_host_config(config_path)?;
    match config.resolve(config_path)? {
        ResolvedFast3dHostAction::Viewer {
            manifest_path,
            gameplay_state_path,
            shader_overrides_path,
        } => {
            let runtime = Fast3dRuntime::load_from_path(&manifest_path)?;
            let runtime_bindings =
                load_runtime_bindings(gameplay_state_path.as_deref(), shader_overrides_path.as_deref())?;
            launch_viewer(manifest_path, runtime, runtime_bindings)?;
        }
        ResolvedFast3dHostAction::Snapshot {
            manifest_path,
            output_path,
            time_seconds,
            gameplay_state_path,
            shader_overrides_path,
        } => {
            let runtime = Fast3dRuntime::load_from_path(&manifest_path)?;
            let runtime_bindings =
                load_runtime_bindings(gameplay_state_path.as_deref(), shader_overrides_path.as_deref())?;
            let frame = runtime.render_frame(
                time_seconds,
                &runtime.default_camera_controls(),
                Some(&runtime_bindings),
                None,
            )?;
            write_snapshot_png(&output_path, &frame)?;
            println!(
                "Wrote Fast3D snapshot to {} ({}x{})",
                output_path.display(),
                frame.width,
                frame.height
            );
        }
        ResolvedFast3dHostAction::ExtractSm64TitleFace {
            sm64_source_root,
            manifest_output_path,
        } => {
            extract_sm64_title_face_scene(&sm64_source_root, &manifest_output_path)?;
            println!(
                "Wrote extracted SM64 title-face scene manifest to {}",
                manifest_output_path.display()
            );
        }
        ResolvedFast3dHostAction::ExtractSm64LevelChunk {
            sm64_source_root,
            level_name,
            area_id,
            manifest_output_path,
        } => {
            extract_sm64_level_chunk_scene(
                &sm64_source_root,
                &level_name,
                area_id,
                &manifest_output_path,
            )?;
            println!(
                "Wrote extracted SM64 {} area {} level chunk manifest to {}",
                level_name,
                area_id,
                manifest_output_path.display()
            );
        }
    }
    Ok(())
}

fn load_runtime_bindings(
    gameplay_state_path: Option<&Path>,
    shader_overrides_path: Option<&Path>,
) -> Result<RuntimeFrameBindings, Box<dyn std::error::Error>> {
    Ok(RuntimeFrameBindings {
        gameplay_state: gameplay_state_path
            .map(load_gameplay_state_document)
            .transpose()?,
        shader_overrides: shader_overrides_path
            .map(load_shader_override_document)
            .transpose()?
            .unwrap_or_default(),
    })
}
