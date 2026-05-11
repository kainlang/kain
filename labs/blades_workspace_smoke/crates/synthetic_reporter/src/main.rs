mod atlas;

use atlas::{collect_shader_artifacts, render_html, render_ppm, render_report, render_svg};
use kain_blades::discover_workspace;
use kain_fs as kfs;
use std::env;
use std::error::Error;
use std::path::PathBuf;

#[derive(Debug, Clone)]
struct Options {
    workspace: PathBuf,
    output: PathBuf,
}

fn main() {
    if let Err(error) = run() {
        eprintln!("blade_singularity_atlas failed: {error}");
        std::process::exit(1);
    }
}

fn run() -> Result<(), Box<dyn Error>> {
    let options = parse_options()?;
    let workspace = discover_workspace(&options.workspace)?;
    let gpu_blade = workspace
        .find_blade("gpu-compute")
        .ok_or("gpu-compute blade was not discovered")?;

    let artifact_root = workspace.root.join(".kain").join("build");
    let artifacts = collect_shader_artifacts(
        &artifact_root,
        &gpu_blade.gpu_shader_sources,
        &gpu_blade.compute_keys,
    )?;
    if artifacts.len() < gpu_blade.gpu_shader_sources.len() {
        return Err(format!(
            "expected at least {} shader artifact sets, found {}",
            gpu_blade.gpu_shader_sources.len(),
            artifacts.len()
        )
        .into());
    }

    kfs::create_dir_all(&options.output)?;
    let svg = render_svg(&workspace.blades, &artifacts);
    let ppm = render_ppm(&artifacts, 384, 216);
    let report = render_report(&workspace, &artifacts);
    let html = render_html(&svg, &report);

    kfs::atomic_write_text(options.output.join("atlas.svg"), &svg)?;
    kfs::atomic_write_bytes(options.output.join("atlas.ppm"), &ppm)?;
    kfs::atomic_write_text(
        options.output.join("atlas.json"),
        &serde_json::to_string_pretty(&report)?,
    )?;
    kfs::atomic_write_text(options.output.join("index.html"), &html)?;

    println!(
        "blade-singularity-atlas shaders={} blades={} spirv_bytes={} output={}",
        artifacts.len(),
        workspace.blades.len(),
        artifacts
            .iter()
            .map(|artifact| artifact.spirv_bytes)
            .sum::<usize>(),
        options.output.display()
    );
    Ok(())
}

fn parse_options() -> Result<Options, Box<dyn Error>> {
    let mut workspace = PathBuf::from(".");
    let mut output = PathBuf::from("outputs/singularity-atlas");
    let mut args = env::args().skip(1);
    while let Some(arg) = args.next() {
        match arg.as_str() {
            "--workspace" => {
                workspace = PathBuf::from(args.next().ok_or("--workspace requires a path")?);
            }
            "--output" => {
                output = PathBuf::from(args.next().ok_or("--output requires a path")?);
            }
            "--help" | "-h" => {
                println!("blade_singularity_atlas --workspace PATH --output PATH");
                std::process::exit(0);
            }
            other => return Err(format!("unknown argument: {other}").into()),
        }
    }
    Ok(Options { workspace, output })
}
