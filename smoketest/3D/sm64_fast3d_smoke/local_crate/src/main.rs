mod combiner;
mod math;
mod model;
mod rasterizer;
mod runtime;
mod texture;
mod viewer;

use std::{env, path::PathBuf};

use runtime::Fast3dRuntime;
use viewer::{launch_viewer, write_snapshot_png, OrbitControls};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let mut args = env::args().skip(1);
    let manifest_path = args
        .next()
        .map(PathBuf::from)
        .unwrap_or_else(|| PathBuf::from("scene_manifest.json"));

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
