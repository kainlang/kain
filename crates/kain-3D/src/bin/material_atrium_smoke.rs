use std::env;
use std::error::Error;
use std::fs::{self, File};
use std::path::{Path, PathBuf};

use font8x8::UnicodeFonts;
use kain_3d::{
    RenderResolution, SceneCatalog, SceneResolutionKind, SoftwareRenderer, SoftwareRendererConfig,
};
use serde_json::json;

const DEFAULT_OUTPUT_IMAGE: &str =
    "smoketest/3D/material_atrium_showcase/material_atrium_visual_example.png";
const DEFAULT_OUTPUT_JSON: &str =
    "smoketest/3D/material_atrium_showcase/generated/material_atrium_runtime_matrix.json";
const DEFAULT_SCENE_NAME: &str = "material_atrium";

struct SmokeConfig {
    output_image: PathBuf,
    output_json: PathBuf,
    scene_name: String,
    width: usize,
    height: usize,
}

#[derive(Clone, Copy)]
struct TileSpec {
    backend_id: &'static str,
    accent: [u8; 4],
    runtime_status: &'static str,
    executor_summary: &'static str,
    note_line: &'static str,
    time_seconds: f32,
    renderer_config: SoftwareRendererConfig,
}

struct TileRender {
    spec: TileSpec,
    frame: kain_3d::RenderFrame,
}

fn main() -> Result<(), Box<dyn Error>> {
    let config = parse_args()?;
    let catalog = SceneCatalog::default();
    let resolved_scene = catalog.resolve_scene(&config.scene_name).ok_or_else(|| {
        format!(
            "scene `{}` is not registered in SceneCatalog::default(); available scenes: {}; aliases: {}",
            config.scene_name,
            catalog.scene_names().join(", "),
            catalog
                .scene_aliases()
                .into_iter()
                .map(|(alias, canonical)| format!("{alias}->{canonical}"))
                .collect::<Vec<_>>()
                .join(", ")
        )
    })?;
    let scene = resolved_scene.scene;

    let header_height = 112usize;
    let gutter = 24usize;
    let card_height = ((config.height - header_height) - (gutter * 3)) / 2;
    let card_width = (config.width - (gutter * 3)) / 2;
    let viewport_height = card_height.saturating_sub(92).max(240);
    let viewport_resolution = RenderResolution::new(card_width.saturating_sub(20), viewport_height);

    let tile_specs = tile_specs();
    let mut rendered_tiles = Vec::with_capacity(tile_specs.len());
    for spec in tile_specs {
        let mut renderer = SoftwareRenderer::default();
        renderer.config = spec.renderer_config;
        let frame = renderer.render_catalog_scene(
            &catalog,
            &config.scene_name,
            spec.time_seconds,
            viewport_resolution,
        )?;
        rendered_tiles.push(TileRender { spec, frame });
    }

    let mut canvas = vec![0u8; config.width * config.height * 4];
    fill_vertical_gradient(
        &mut canvas,
        config.width,
        config.height,
        [11, 16, 23, 255],
        [28, 20, 27, 255],
    );

    draw_text_block(
        &mut canvas,
        config.width,
        config.height,
        28,
        24,
        3,
        [246, 244, 236, 255],
        "KAIN 3D RUNTIME MATRIX",
    );
    draw_text_block(
        &mut canvas,
        config.width,
        config.height,
        30,
        58,
        2,
        [168, 191, 214, 255],
        "material_atrium | live runtime backend identity with deterministic software preview",
    );
    draw_text_block(
        &mut canvas,
        config.width,
        config.height,
        30,
        82,
        2,
        [221, 190, 146, 255],
        "bgfx is compile-backed; the others remain staged until viewport bridges land.",
    );

    for (index, tile) in rendered_tiles.iter().enumerate() {
        let row = index / 2;
        let column = index % 2;
        let x = gutter + column * (card_width + gutter);
        let y = header_height + gutter + row * (card_height + gutter);
        draw_tile(
            &mut canvas,
            config.width,
            config.height,
            x,
            y,
            card_width,
            card_height,
            tile,
        );
    }

    write_png(&config.output_image, config.width, config.height, &canvas)?;
    write_report(
        &config,
        scene.viewport_summary.as_str(),
        &resolved_scene.resolution,
        &rendered_tiles,
    )?;

    println!(
        "material_atrium smoke wrote {} and {}",
        config.output_image.display(),
        config.output_json.display()
    );
    Ok(())
}

fn parse_args() -> Result<SmokeConfig, Box<dyn Error>> {
    let mut output_image = PathBuf::from(DEFAULT_OUTPUT_IMAGE);
    let mut output_json = PathBuf::from(DEFAULT_OUTPUT_JSON);
    let mut scene_name = DEFAULT_SCENE_NAME.to_string();
    let mut width = 1600usize;
    let mut height = 1040usize;

    let mut args = env::args().skip(1);
    while let Some(argument) = args.next() {
        match argument.as_str() {
            "--output-image" => {
                output_image =
                    PathBuf::from(args.next().ok_or("--output-image requires a path value")?);
            }
            "--output-json" => {
                output_json =
                    PathBuf::from(args.next().ok_or("--output-json requires a path value")?);
            }
            "--scene" => {
                scene_name = args.next().ok_or("--scene requires a scene name")?;
            }
            "--width" => {
                width = args
                    .next()
                    .ok_or("--width requires a value")?
                    .parse::<usize>()?
                    .max(960);
            }
            "--height" => {
                height = args
                    .next()
                    .ok_or("--height requires a value")?
                    .parse::<usize>()?
                    .max(720);
            }
            "--help" | "-h" => {
                print_help();
                std::process::exit(0);
            }
            other => {
                return Err(format!("unknown argument `{other}`").into());
            }
        }
    }

    Ok(SmokeConfig {
        output_image,
        output_json,
        scene_name,
        width,
        height,
    })
}

fn print_help() {
    println!("material_atrium_smoke");
    println!("  --output-image <path>");
    println!("  --output-json <path>");
    println!("  --scene <scene-name>");
    println!("  --width <pixels>");
    println!("  --height <pixels>");
}

fn tile_specs() -> [TileSpec; 4] {
    [
        TileSpec {
            backend_id: "bgfx",
            accent: [89, 189, 255, 255],
            runtime_status: "compile-backed runtime lane",
            executor_summary: "native host currently renders via compatibility executor",
            note_line: "cross-platform baseline backend for viewport/device/swapchain work",
            time_seconds: 0.35,
            renderer_config: SoftwareRendererConfig {
                wireframe_overlay: false,
                rim_light_strength: 0.18,
            },
        },
        TileSpec {
            backend_id: "filament",
            accent: [255, 204, 128, 255],
            runtime_status: "staged premium renderer lane",
            executor_summary: "visual/material bridge still pending native viewport execution",
            note_line: "high-end material, lighting, and premium presentation target",
            time_seconds: 1.10,
            renderer_config: SoftwareRendererConfig {
                wireframe_overlay: false,
                rim_light_strength: 0.26,
            },
        },
        TileSpec {
            backend_id: "diligent",
            accent: [154, 232, 197, 255],
            runtime_status: "staged explicit renderer lane",
            executor_summary: "render-graph and compute bridge remain ahead of viewport hookup",
            note_line: "future Kain-owned graph/pipeline/control depth lane",
            time_seconds: 1.85,
            renderer_config: SoftwareRendererConfig {
                wireframe_overlay: true,
                rim_light_strength: 0.16,
            },
        },
        TileSpec {
            backend_id: "the-forge",
            accent: [242, 140, 140, 255],
            runtime_status: "staged low-level renderer lane",
            executor_summary:
                "bridge-first backend identity is cataloged, viewport path still pending",
            note_line: "aggressive low-level GPU substrate for future Kain renderer expansion",
            time_seconds: 2.45,
            renderer_config: SoftwareRendererConfig {
                wireframe_overlay: false,
                rim_light_strength: 0.22,
            },
        },
    ]
}

fn draw_tile(
    canvas: &mut [u8],
    canvas_width: usize,
    canvas_height: usize,
    x: usize,
    y: usize,
    width: usize,
    height: usize,
    tile: &TileRender,
) {
    let viewport_x = x + 10;
    let viewport_y = y + 52;
    let footer_y = y + height.saturating_sub(76);
    let frame = &tile.frame;

    fill_rect(
        canvas,
        canvas_width,
        canvas_height,
        x,
        y,
        width,
        height,
        [18, 23, 31, 255],
    );
    stroke_rect(
        canvas,
        canvas_width,
        canvas_height,
        x,
        y,
        width,
        height,
        tile.spec.accent,
    );
    fill_rect(
        canvas,
        canvas_width,
        canvas_height,
        x + 1,
        y + 1,
        width.saturating_sub(2),
        40,
        scale_alpha(tile.spec.accent, 0.22),
    );

    blit_rgba(
        canvas,
        canvas_width,
        canvas_height,
        viewport_x,
        viewport_y,
        frame.width,
        frame.height,
        &frame.rgba,
    );

    stroke_rect(
        canvas,
        canvas_width,
        canvas_height,
        viewport_x.saturating_sub(1),
        viewport_y.saturating_sub(1),
        frame.width + 2,
        frame.height + 2,
        [255, 255, 255, 56],
    );

    fill_rect(
        canvas,
        canvas_width,
        canvas_height,
        x + 10,
        footer_y,
        width.saturating_sub(20),
        58,
        [14, 16, 22, 235],
    );

    draw_text_block(
        canvas,
        canvas_width,
        canvas_height,
        (x + 14) as u32,
        (y + 12) as u32,
        2,
        [244, 244, 238, 255],
        tile.spec.backend_id,
    );
    draw_text_block(
        canvas,
        canvas_width,
        canvas_height,
        (x + 14) as u32,
        (y + 30) as u32,
        1,
        tile.spec.accent,
        tile.spec.runtime_status,
    );
    draw_text_block(
        canvas,
        canvas_width,
        canvas_height,
        (x + 14) as u32,
        (footer_y + 8) as u32,
        1,
        [216, 223, 230, 255],
        tile.spec.executor_summary,
    );
    draw_text_block(
        canvas,
        canvas_width,
        canvas_height,
        (x + 14) as u32,
        (footer_y + 24) as u32,
        1,
        [160, 173, 184, 255],
        tile.spec.note_line,
    );

    let stats_line = format!(
        "tris {} | shaded {} | particles {}",
        tile.frame.stats.triangles_rasterized,
        tile.frame.stats.pixels_shaded,
        tile.frame.stats.particles_shaded
    );
    draw_text_block(
        canvas,
        canvas_width,
        canvas_height,
        (x + 14) as u32,
        (footer_y + 40) as u32,
        1,
        [124, 137, 148, 255],
        &stats_line,
    );
}

fn write_report(
    config: &SmokeConfig,
    viewport_summary: &str,
    resolution: &kain_3d::SceneResolution,
    rendered_tiles: &[TileRender],
) -> Result<(), Box<dyn Error>> {
    if let Some(parent) = config.output_json.parent() {
        fs::create_dir_all(parent)?;
    }

    let report = json!({
        "date": "2026-04-11",
        "scene": config.scene_name,
        "scene_resolution": {
            "requested_name": resolution.requested_name,
            "resolved_name": resolution.resolved_name,
            "kind": match &resolution.kind {
                SceneResolutionKind::Exact => "exact",
                SceneResolutionKind::Alias { .. } => "alias",
                SceneResolutionKind::Default { .. } => "default",
            },
        },
        "viewport_summary": viewport_summary,
        "output_image": config.output_image,
        "output_json": config.output_json,
        "canvas": {
            "width": config.width,
            "height": config.height
        },
        "tiles": rendered_tiles.iter().map(|tile| {
            json!({
                "backend_id": tile.spec.backend_id,
                "runtime_status": tile.spec.runtime_status,
                "executor_summary": tile.spec.executor_summary,
                "note_line": tile.spec.note_line,
                "time_seconds": tile.spec.time_seconds,
                "frame": {
                    "width": tile.frame.width,
                    "height": tile.frame.height,
                    "triangles_submitted": tile.frame.stats.triangles_submitted,
                    "triangles_rasterized": tile.frame.stats.triangles_rasterized,
                    "pixels_shaded": tile.frame.stats.pixels_shaded,
                    "particles_submitted": tile.frame.stats.particles_submitted,
                    "particles_shaded": tile.frame.stats.particles_shaded
                },
                "diagnostics": {
                    "camera_source": tile.frame.diagnostics.camera_source.as_ref().map(|source| match source {
                        kain_3d::FrameCameraSource::ExplicitView => "explicit_view",
                        kain_3d::FrameCameraSource::AutoFramed => "auto_framed",
                    }),
                    "scene_name": tile.frame.diagnostics.scene_name,
                    "viewport_resolution": tile.frame.diagnostics.viewport_resolution,
                    "viewport_summary": tile.frame.diagnostics.viewport_summary,
                    "composition_summary": tile.frame.diagnostics.composition_summary,
                    "scene_shape": tile.frame.diagnostics.scene_shape,
                    "visible_instances": tile.frame.diagnostics.visible_instances,
                    "culled_instances": tile.frame.diagnostics.culled_instances,
                }
            })
        }).collect::<Vec<_>>()
    });

    fs::write(&config.output_json, serde_json::to_vec_pretty(&report)?)?;
    Ok(())
}

fn write_png(path: &Path, width: usize, height: usize, rgba: &[u8]) -> Result<(), Box<dyn Error>> {
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent)?;
    }

    let file = File::create(path)?;
    let mut encoder = png::Encoder::new(file, width as u32, height as u32);
    encoder.set_color(png::ColorType::Rgba);
    encoder.set_depth(png::BitDepth::Eight);
    let mut writer = encoder.write_header()?;
    writer.write_image_data(rgba)?;
    Ok(())
}

fn fill_vertical_gradient(
    canvas: &mut [u8],
    width: usize,
    height: usize,
    top: [u8; 4],
    bottom: [u8; 4],
) {
    for y in 0..height {
        let t = if height <= 1 {
            0.0
        } else {
            y as f32 / (height - 1) as f32
        };
        let color = lerp_color(top, bottom, t);
        fill_rect(canvas, width, height, 0, y, width, 1, color);
    }
}

fn fill_rect(
    canvas: &mut [u8],
    canvas_width: usize,
    canvas_height: usize,
    x: usize,
    y: usize,
    width: usize,
    height: usize,
    color: [u8; 4],
) {
    let max_x = (x + width).min(canvas_width);
    let max_y = (y + height).min(canvas_height);
    for py in y.min(canvas_height)..max_y {
        for px in x.min(canvas_width)..max_x {
            let index = (py * canvas_width + px) * 4;
            canvas[index..index + 4].copy_from_slice(&color);
        }
    }
}

fn stroke_rect(
    canvas: &mut [u8],
    canvas_width: usize,
    canvas_height: usize,
    x: usize,
    y: usize,
    width: usize,
    height: usize,
    color: [u8; 4],
) {
    fill_rect(canvas, canvas_width, canvas_height, x, y, width, 1, color);
    fill_rect(
        canvas,
        canvas_width,
        canvas_height,
        x,
        y + height.saturating_sub(1),
        width,
        1,
        color,
    );
    fill_rect(canvas, canvas_width, canvas_height, x, y, 1, height, color);
    fill_rect(
        canvas,
        canvas_width,
        canvas_height,
        x + width.saturating_sub(1),
        y,
        1,
        height,
        color,
    );
}

fn blit_rgba(
    canvas: &mut [u8],
    canvas_width: usize,
    canvas_height: usize,
    dst_x: usize,
    dst_y: usize,
    src_width: usize,
    src_height: usize,
    src_rgba: &[u8],
) {
    for row in 0..src_height {
        let canvas_y = dst_y + row;
        if canvas_y >= canvas_height {
            break;
        }
        for column in 0..src_width {
            let canvas_x = dst_x + column;
            if canvas_x >= canvas_width {
                break;
            }
            let src_index = (row * src_width + column) * 4;
            let dst_index = (canvas_y * canvas_width + canvas_x) * 4;
            canvas[dst_index..dst_index + 4].copy_from_slice(&src_rgba[src_index..src_index + 4]);
        }
    }
}

fn draw_text_block(
    canvas: &mut [u8],
    canvas_width: usize,
    canvas_height: usize,
    x: u32,
    y: u32,
    scale: u32,
    color: [u8; 4],
    text: &str,
) {
    let shadow_color = [0, 0, 0, 180];
    draw_text(
        canvas,
        canvas_width,
        canvas_height,
        x + scale,
        y + scale,
        scale,
        shadow_color,
        text,
    );
    draw_text(
        canvas,
        canvas_width,
        canvas_height,
        x,
        y,
        scale,
        color,
        text,
    );
}

fn draw_text(
    canvas: &mut [u8],
    canvas_width: usize,
    canvas_height: usize,
    start_x: u32,
    start_y: u32,
    scale: u32,
    color: [u8; 4],
    text: &str,
) {
    let scale = scale.max(1);
    let mut cursor_x = start_x;
    for character in text.chars() {
        if character == ' ' {
            cursor_x += 6 * scale;
            continue;
        }
        if let Some(glyph) = font8x8::BASIC_FONTS.get(character) {
            for (row_index, row_bits) in glyph.iter().enumerate() {
                for column_index in 0..8u32 {
                    if (row_bits >> column_index) & 1 == 0 {
                        continue;
                    }
                    for scale_y in 0..scale {
                        for scale_x in 0..scale {
                            let px = cursor_x + column_index * scale + scale_x;
                            let py = start_y + row_index as u32 * scale + scale_y;
                            if px as usize >= canvas_width || py as usize >= canvas_height {
                                continue;
                            }
                            let index = (py as usize * canvas_width + px as usize) * 4;
                            canvas[index..index + 4].copy_from_slice(&color);
                        }
                    }
                }
            }
        }
        cursor_x += 9 * scale;
    }
}

fn lerp_color(a: [u8; 4], b: [u8; 4], t: f32) -> [u8; 4] {
    let t = t.clamp(0.0, 1.0);
    [
        lerp_channel(a[0], b[0], t),
        lerp_channel(a[1], b[1], t),
        lerp_channel(a[2], b[2], t),
        lerp_channel(a[3], b[3], t),
    ]
}

fn lerp_channel(a: u8, b: u8, t: f32) -> u8 {
    let start = a as f32;
    let end = b as f32;
    (start + ((end - start) * t)).round().clamp(0.0, 255.0) as u8
}

fn scale_alpha(color: [u8; 4], alpha_scale: f32) -> [u8; 4] {
    [
        color[0],
        color[1],
        color[2],
        (color[3] as f32 * alpha_scale.clamp(0.0, 1.0)).round() as u8,
    ]
}
