use kain_blades::{BladeWorkspace, ResolvedBlade};
use kain_fs::{self as kfs, FsFileType, WalkOptions};
use serde_json::json;
use std::error::Error;
use std::path::{Path, PathBuf};

#[derive(Debug, Clone)]
pub struct ShaderArtifact {
    source_name: String,
    compute_key: String,
    spirv_path: PathBuf,
    pub spirv_bytes: usize,
    spirv_hash: String,
    reflect_path: Option<PathBuf>,
    bundle_path: Option<PathBuf>,
    hlsl_path: Option<PathBuf>,
}

pub fn collect_shader_artifacts(
    artifact_root: &Path,
    shader_sources: &[PathBuf],
    compute_keys: &[String],
) -> Result<Vec<ShaderArtifact>, Box<dyn Error>> {
    let entries = if artifact_root.exists() {
        kfs::walk_dir_entries(
            artifact_root,
            WalkOptions {
                include_dirs: false,
                include_files: true,
                ..WalkOptions::default()
            },
        )?
    } else {
        Vec::new()
    };
    let mut artifacts = Vec::new();
    for (index, source) in shader_sources.iter().enumerate() {
        let source_name = source
            .file_stem()
            .and_then(|name| name.to_str())
            .unwrap_or("shader")
            .to_string();
        let Some(spirv_entry) = entries.iter().find(|entry| {
            entry.file_type == FsFileType::File && entry.file_name == format!("{source_name}.spv")
        }) else {
            continue;
        };
        let spirv_path = spirv_entry.path.clone();
        let spirv_bytes = kfs::read_bytes(&spirv_path)?.len();
        let spirv_hash = kfs::hash_file(&spirv_path)?;
        artifacts.push(ShaderArtifact {
            source_name: source_name.clone(),
            compute_key: compute_keys
                .get(index)
                .cloned()
                .unwrap_or_else(|| source_name.clone()),
            reflect_path: find_sibling(&entries, &source_name, ".reflect.json"),
            bundle_path: find_sibling(&entries, &source_name, ".shader_bundle.json"),
            hlsl_path: find_sibling(&entries, &source_name, ".hlsl"),
            spirv_path,
            spirv_bytes,
            spirv_hash,
        });
    }
    Ok(artifacts)
}

pub fn render_report(
    workspace: &BladeWorkspace,
    artifacts: &[ShaderArtifact],
) -> serde_json::Value {
    json!({
        "title": "Blade Singularity Atlas",
        "workspace": workspace.root,
        "blade_count": workspace.blades.len(),
        "shader_count": artifacts.len(),
        "total_spirv_bytes": artifacts.iter().map(|artifact| artifact.spirv_bytes).sum::<usize>(),
        "compute_keys": artifacts.iter().map(|artifact| artifact.compute_key.clone()).collect::<Vec<_>>(),
        "shaders": artifacts.iter().map(|artifact| {
            json!({
                "source": artifact.source_name,
                "compute_key": artifact.compute_key,
                "spirv_path": artifact.spirv_path,
                "spirv_bytes": artifact.spirv_bytes,
                "spirv_hash": artifact.spirv_hash,
                "has_reflection": artifact.reflect_path.is_some(),
                "has_shader_bundle": artifact.bundle_path.is_some(),
                "has_hlsl": artifact.hlsl_path.is_some(),
            })
        }).collect::<Vec<_>>(),
        "blades": workspace.blades.iter().map(|blade| {
            json!({
                "name": blade.name,
                "kind": blade.kind,
                "dependencies": blade.dependencies.iter().map(|dep| dep.name.clone()).collect::<Vec<_>>(),
            })
        }).collect::<Vec<_>>(),
    })
}

pub fn render_svg(blades: &[ResolvedBlade], artifacts: &[ShaderArtifact]) -> String {
    let mut svg = String::from(
        r##"<svg xmlns="http://www.w3.org/2000/svg" width="960" height="540" viewBox="0 0 960 540">
<defs>
<linearGradient id="bg" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#06120f"/><stop offset="0.47" stop-color="#162016"/><stop offset="1" stop-color="#230b12"/></linearGradient>
<filter id="glow"><feGaussianBlur stdDeviation="3.5" result="blur"/><feMerge><feMergeNode in="blur"/><feMergeNode in="SourceGraphic"/></feMerge></filter>
</defs>
<rect width="960" height="540" fill="url(#bg)"/>
<rect x="28" y="28" width="904" height="484" rx="18" fill="none" stroke="#f2d47c" stroke-opacity="0.32"/>
<text x="54" y="72" fill="#f8e8b0" font-family="Consolas, monospace" font-size="30" font-weight="700">BLADE SINGULARITY ATLAS</text>
<text x="56" y="103" fill="#89f0d0" font-family="Consolas, monospace" font-size="14">SPIR-V artifacts forged by the GPU blade workspace</text>
"##,
    );

    for (index, artifact) in artifacts.iter().enumerate() {
        let x = 90 + index as i32 * 260;
        let hash_bytes = artifact.spirv_hash.as_bytes();
        let color_a = color_from_hash(hash_bytes, 0);
        let color_b = color_from_hash(hash_bytes, 9);
        svg.push_str(&format!(
            r##"<g filter="url(#glow)">
<circle cx="{x}" cy="250" r="78" fill="{color_a}" fill-opacity="0.32"/>
<circle cx="{x}" cy="250" r="42" fill="{color_b}" fill-opacity="0.78"/>
<path d="M{x0} 250 C{x1} {y1}, {x2} {y2}, {x3} 250" fill="none" stroke="{color_b}" stroke-width="5" stroke-opacity="0.82"/>
</g>
<text x="{tx}" y="370" fill="#f8e8b0" font-family="Consolas, monospace" font-size="16" text-anchor="middle">{key}</text>
<text x="{tx}" y="393" fill="#aee9da" font-family="Consolas, monospace" font-size="12" text-anchor="middle">{bytes} bytes SPIR-V</text>
"##,
            x = x,
            x0 = x - 82,
            x1 = x - 34,
            y1 = 168 + (hash_bytes.get(1).copied().unwrap_or(3) as i32 % 36),
            x2 = x + 42,
            y2 = 322 - (hash_bytes.get(4).copied().unwrap_or(5) as i32 % 52),
            x3 = x + 90,
            tx = x,
            key = artifact.compute_key,
            bytes = artifact.spirv_bytes,
        ));
    }

    for (index, blade) in blades.iter().enumerate() {
        let x = 62 + (index % 4) as i32 * 215;
        let y = 438 + (index / 4) as i32 * 30;
        svg.push_str(&format!(
            r##"<text x="{x}" y="{y}" fill="#d9f4e9" font-family="Consolas, monospace" font-size="13">{name}:{kind}</text>
"##,
            name = blade.name,
            kind = blade.kind,
        ));
    }
    svg.push_str("</svg>\n");
    svg
}

pub fn render_html(svg: &str, report: &serde_json::Value) -> String {
    format!(
        r#"<!doctype html>
<html lang="en">
<meta charset="utf-8">
<title>Blade Singularity Atlas</title>
<style>
html,body{{margin:0;background:#06120f;color:#f8e8b0;font:14px Consolas,monospace}}
main{{min-height:100vh;display:grid;place-items:center;padding:28px}}
section{{width:min(1120px,100%)}}
svg{{width:100%;height:auto;display:block;border:1px solid rgba(242,212,124,.24)}}
pre{{white-space:pre-wrap;color:#aee9da;background:#0a1512;padding:18px;border:1px solid rgba(137,240,208,.22)}}
</style>
<main><section>{svg}<pre>{report}</pre></section></main>
</html>
"#,
        svg = svg,
        report = serde_json::to_string_pretty(report).unwrap_or_else(|_| "{}".to_string())
    )
}

pub fn render_ppm(artifacts: &[ShaderArtifact], width: usize, height: usize) -> Vec<u8> {
    let mut bytes = format!("P6\n{width} {height}\n255\n").into_bytes();
    for y in 0..height {
        for x in 0..width {
            let artifact = &artifacts[(x * artifacts.len() / width).min(artifacts.len() - 1)];
            let hash = artifact.spirv_hash.as_bytes();
            let h0 = hash.get((x + y) % hash.len()).copied().unwrap_or(b'4') as usize;
            let h1 = hash.get((x * 3 + y) % hash.len()).copied().unwrap_or(b'8') as usize;
            let wave = ((x ^ y ^ h0) & 0xff) as u8;
            let flare = ((x * 5 + y * 2 + h1) & 0xff) as u8;
            let depth = ((height - y + artifact.spirv_bytes) & 0xff) as u8;
            bytes.extend_from_slice(&[wave, flare, depth]);
        }
    }
    bytes
}

fn find_sibling(
    entries: &[kain_fs::DirectoryEntry],
    source_name: &str,
    suffix: &str,
) -> Option<PathBuf> {
    let file_name = format!("{source_name}{suffix}");
    entries
        .iter()
        .find(|entry| entry.file_type == FsFileType::File && entry.file_name == file_name)
        .map(|entry| entry.path.clone())
}

fn color_from_hash(hash: &[u8], offset: usize) -> String {
    let r = 70 + hash.get(offset).copied().unwrap_or(b'a') % 150;
    let g = 70 + hash.get(offset + 1).copied().unwrap_or(b'b') % 160;
    let b = 70 + hash.get(offset + 2).copied().unwrap_or(b'c') % 150;
    format!("#{r:02x}{g:02x}{b:02x}")
}
