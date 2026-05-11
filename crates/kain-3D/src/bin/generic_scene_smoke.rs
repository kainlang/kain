use std::collections::BTreeMap;
use std::env;
use std::error::Error;
use std::fs::{self, File};
use std::path::{Path, PathBuf};

use kain_3d::{
    BackgroundGradient, Camera, ColorRgb, DirectionalLight, Geometry, LightingRig, Material,
    RenderResolution, SceneCatalog, SceneDescription, SceneInstance, SoftwareRenderer, Transform,
    Vec3,
};
use serde_json::json;

const DEFAULT_OUTPUT_IMAGE: &str =
    "smoketest/3D/generic_scene_smoke/generic_scene_visual_reference.png";
const DEFAULT_OUTPUT_JSON: &str =
    "smoketest/3D/generic_scene_smoke/generated/generic_scene_runtime_report.json";
const DEFAULT_SCENE_NAME: &str = "geometry_fixture";

struct SmokeConfig {
    output_image: PathBuf,
    output_json: PathBuf,
    scene_name: String,
    width: usize,
    height: usize,
}

fn main() -> Result<(), Box<dyn Error>> {
    let config = parse_args()?;
    let catalog = generic_fixture_catalog()?;
    let resolved_scene = catalog.resolve_scene(&config.scene_name).ok_or_else(|| {
        format!(
            "scene `{}` is not registered in the explicit smoke catalog",
            config.scene_name
        )
    })?;

    let mut renderer = SoftwareRenderer::default();
    let resolution = RenderResolution::new(config.width, config.height);
    let frame = renderer.render_catalog_scene(&catalog, &config.scene_name, 0.0, resolution)?;

    write_png(&config.output_image, frame.width, frame.height, &frame.rgba)?;
    write_report(&config, &catalog, resolved_scene.scene, &frame)?;

    println!(
        "generic 3d scene smoke wrote {} and {}",
        config.output_image.display(),
        config.output_json.display()
    );
    Ok(())
}

fn parse_args() -> Result<SmokeConfig, Box<dyn Error>> {
    let mut output_image = PathBuf::from(DEFAULT_OUTPUT_IMAGE);
    let mut output_json = PathBuf::from(DEFAULT_OUTPUT_JSON);
    let mut scene_name = DEFAULT_SCENE_NAME.to_string();
    let mut width = 960usize;
    let mut height = 640usize;

    let mut args = env::args().skip(1);
    while let Some(argument) = args.next() {
        match argument.as_str() {
            "--output-image" => {
                output_image = PathBuf::from(args.next().ok_or("--output-image requires a path")?);
            }
            "--output-json" => {
                output_json = PathBuf::from(args.next().ok_or("--output-json requires a path")?);
            }
            "--scene" => {
                scene_name = args.next().ok_or("--scene requires a scene name")?;
            }
            "--width" => {
                width = args.next().ok_or("--width requires a value")?.parse()?;
            }
            "--height" => {
                height = args.next().ok_or("--height requires a value")?.parse()?;
            }
            "--help" | "-h" => {
                print_help();
                std::process::exit(0);
            }
            unknown => return Err(format!("unknown argument `{unknown}`").into()),
        }
    }

    Ok(SmokeConfig {
        output_image,
        output_json,
        scene_name,
        width: width.max(1),
        height: height.max(1),
    })
}

fn print_help() {
    println!("generic_scene_smoke [--output-image PATH] [--output-json PATH] [--scene NAME] [--width PX] [--height PX]");
}

fn generic_fixture_catalog() -> Result<SceneCatalog, Box<dyn Error>> {
    let scene = generic_fixture_scene()?;
    Ok(SceneCatalog::new(
        scene.name.clone(),
        BTreeMap::from([(scene.name.clone(), scene)]),
        BTreeMap::from([(
            "fixture_preview".to_string(),
            DEFAULT_SCENE_NAME.to_string(),
        )]),
    )?)
}

fn generic_fixture_scene() -> Result<SceneDescription, Box<dyn Error>> {
    let cube = Geometry::box_mesh(Vec3::new(2.0, 2.0, 2.0)).to_mesh()?;
    let floor = Geometry::plane(kain_3d::Vec2::new(8.0, 8.0)).to_mesh()?;

    let mut meshes = BTreeMap::new();
    meshes.insert("cube".to_string(), cube);
    meshes.insert("floor".to_string(), floor);

    let mut materials = BTreeMap::new();
    materials.insert(
        "matte_blue".to_string(),
        Material {
            base_color: ColorRgb::new(0.18, 0.48, 0.82),
            specular_color: ColorRgb::WHITE,
            ambient_strength: 0.25,
            diffuse_strength: 0.9,
            specular_strength: 0.18,
            shininess: 18.0,
        },
    );
    materials.insert(
        "matte_floor".to_string(),
        Material {
            base_color: ColorRgb::new(0.18, 0.20, 0.23),
            specular_color: ColorRgb::new(0.5, 0.52, 0.56),
            ambient_strength: 0.22,
            diffuse_strength: 0.75,
            specular_strength: 0.08,
            shininess: 6.0,
        },
    );

    Ok(SceneDescription {
        name: DEFAULT_SCENE_NAME.to_string(),
        viewport_summary: "explicit fixture scene assembled by the smoke binary".to_string(),
        background: BackgroundGradient {
            top: ColorRgb::new(0.025, 0.035, 0.055),
            bottom: ColorRgb::new(0.08, 0.095, 0.12),
        },
        camera: Camera {
            target: Vec3::ZERO,
            up: Vec3::UP,
            orbit_radius: 8.0,
            orbit_height: 4.0,
            orbit_speed_radians_per_second: 0.0,
            fov_y_degrees: 55.0,
            near_plane: 0.05,
            far_plane: 120.0,
        },
        lighting: LightingRig {
            ambient_color: ColorRgb::WHITE,
            ambient_intensity: 0.35,
            directional_lights: vec![DirectionalLight {
                direction: Vec3::new(-0.4, -1.0, -0.35).normalize(),
                color: ColorRgb::WHITE,
                intensity: 1.0,
            }],
            point_lights: vec![],
        },
        meshes,
        materials,
        instances: vec![
            SceneInstance {
                id: "floor".to_string(),
                mesh: "floor".to_string(),
                material: "matte_floor".to_string(),
                transform: Transform::identity().with_translation(Vec3::new(0.0, -1.05, 0.0)),
            },
            SceneInstance {
                id: "cube".to_string(),
                mesh: "cube".to_string(),
                material: "matte_blue".to_string(),
                transform: Transform::identity().with_translation(Vec3::new(0.0, 0.0, 0.0)),
            },
        ],
        animations: vec![],
        particle_emitters: vec![],
    })
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

fn write_report(
    config: &SmokeConfig,
    catalog: &SceneCatalog,
    scene: &SceneDescription,
    frame: &kain_3d::RenderFrame,
) -> Result<(), Box<dyn Error>> {
    if let Some(parent) = config.output_json.parent() {
        fs::create_dir_all(parent)?;
    }

    let summary = scene
        .composition_summary_with_aspect_ratio(0.0, config.width as f32 / config.height as f32);
    let diagnostics = summary.diagnostics();
    let bounds = diagnostics.bounds.map(|bounds| {
        let span = bounds.span();
        json!({
            "center": [bounds.center.x, bounds.center.y, bounds.center.z],
            "span": [span.x, span.y, span.z],
            "radius": bounds.radius(),
        })
    });

    let payload = json!({
        "scene": scene.name,
        "catalog": {
            "default_scene": catalog.summary().default_scene,
            "canonical_scene_count": catalog.summary().canonical_scene_count,
            "alias_count": catalog.summary().alias_count,
            "total_scene_names": catalog.summary().total_scene_names,
        },
        "render": {
            "width": frame.width,
            "height": frame.height,
            "triangles_submitted": frame.stats.triangles_submitted,
            "triangles_rasterized": frame.stats.triangles_rasterized,
            "pixels_shaded": frame.stats.pixels_shaded,
            "visible_instances": frame.diagnostics.visible_instances,
            "culled_instances": frame.diagnostics.culled_instances,
            "camera_source": format!("{:?}", frame.diagnostics.camera_source),
            "composition_summary": frame.diagnostics.composition_summary,
            "framing_hint": frame.diagnostics.framing_hint,
            "camera_fit_ratio": frame.diagnostics.camera_fit_ratio,
        },
        "composition": {
            "mesh_count": diagnostics.mesh_count,
            "material_count": diagnostics.material_count,
            "instance_count": diagnostics.instance_count,
            "animation_count": diagnostics.animation_count,
            "particle_emitter_count": diagnostics.particle_emitter_count,
            "directional_light_count": diagnostics.directional_light_count,
            "point_light_count": diagnostics.point_light_count,
            "viewport_aspect_ratio": diagnostics.viewport_aspect_ratio,
            "framed_camera_distance": diagnostics.framed_camera_distance,
            "framing_hint": diagnostics.framing_hint,
            "camera_fit_ratio": diagnostics.camera_fit_ratio,
            "bounds": bounds,
        },
    });

    fs::write(&config.output_json, serde_json::to_string_pretty(&payload)?)?;
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn generic_fixture_catalog_has_explicit_alias_only() {
        let catalog = generic_fixture_catalog().expect("fixture catalog should build");
        assert!(catalog.scene(DEFAULT_SCENE_NAME).is_some());
        assert_eq!(
            catalog.scene("fixture_preview").unwrap().name,
            DEFAULT_SCENE_NAME
        );
        assert!(catalog.scene("missing").is_some());
        assert_eq!(catalog.summary().canonical_scene_count, 1);
        assert_eq!(catalog.summary().alias_count, 1);
    }

    #[test]
    fn generic_fixture_scene_has_structural_composition_payload() {
        let scene = generic_fixture_scene().expect("fixture scene should build");
        let summary = scene.composition_summary_with_aspect_ratio(0.0, 1.5);
        let diagnostics = summary.diagnostics();
        assert_eq!(diagnostics.mesh_count, 2);
        assert_eq!(diagnostics.material_count, 2);
        assert_eq!(diagnostics.instance_count, 2);
        assert!(diagnostics.bounds.is_some());
        assert!(diagnostics.framing_hint.is_some());
        assert!(diagnostics.camera_fit_ratio.is_some());
    }
}
