mod extractor;
mod combiner;
mod math;
mod model;
mod rasterizer;
mod runtime;
mod texture;
mod viewer;

use std::{env, path::PathBuf};

use extractor::extract_sm64_title_face_scene;
use runtime::Fast3dRuntime;
use viewer::{launch_viewer, write_snapshot_png, OrbitControls};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let mut args = env::args().skip(1);
    let Some(first_argument) = args.next() else {
        let runtime = Fast3dRuntime::load_from_path(&PathBuf::from("scene_manifest.json"))?;
        launch_viewer(PathBuf::from("scene_manifest.json"), runtime)?;
        return Ok(());
    };

    if first_argument == "--extract-sm64-title-face" {
        let sm64_root = args
            .next()
            .map(PathBuf::from)
            .ok_or("expected SM64 source root after --extract-sm64-title-face")?;
        let mut manifest_out = PathBuf::from("scene_manifest_title_face.json");
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
    if let Some(snapshot_path) = snapshot_path {
        let frame = runtime.render_frame(snapshot_time_seconds, &OrbitControls::default(), None)?;
        write_snapshot_png(&snapshot_path, &frame)?;
        println!(
            "Wrote Fast3D snapshot to {} ({}x{})",
            snapshot_path.display(),
            frame.width,
            frame.height
        );
        return Ok(());
    }

    launch_viewer(manifest_path, runtime)?;
    Ok(())
}
