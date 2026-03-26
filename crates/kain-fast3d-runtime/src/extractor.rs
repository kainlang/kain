use std::{
    collections::{BTreeMap, HashMap, HashSet},
    fs,
    path::Path,
};

use glam::{Mat4, Vec3};
use regex::Regex;

use crate::model::{
    CameraConfig, CombineMode, DisplayListCommand, DisplayListDefinition, Fast3dSmokeManifest,
    Fast3dVertex, LightGroupDefinition, ResolutionConfig, TextureDefinition, TextureSource,
};

const MARIO_FACE_MODEL_PATH: &str = "actors/mario/model.inc.c";
const TITLE_FACE_ROTATION_Y_RADIANS: f32 = -std::f32::consts::FRAC_PI_2;
const TITLE_FACE_TARGET_WIDTH: f32 = 2.35;
const TITLE_FACE_TARGET_HEIGHT: f32 = 2.1;
const TITLE_FACE_TARGET_CENTER: [f32; 3] = [1.8, -0.05, 0.3];

/// N64 units to normalized world units. SM64 uses 1 unit ≈ 1 cm;
/// Bob-omb Battlefield spans ≈ ±40,000 units, so divide by 500 to get ≈ ±80 world units.
const LEVEL_N64_UNIT_SCALE: f32 = 1.0 / 500.0;

/// Extract a full SM64 level chunk from the decompiled source tree.
///
/// Reads all `model.inc.c` files under `levels/{level_name}/areas/{area_id}/{sub_id}/`
/// for the given area, merges them into a single manifest, and writes it to `manifest_out`.
///
/// Positions are normalized from N64 integer units to world units via `LEVEL_N64_UNIT_SCALE`.
///
/// Textures are placeholder checkerboards since real textures require ROM extraction.
/// UV scale is set to render-reasonable values across the geometry.
pub fn extract_sm64_level_chunk_scene(
    sm64_root: &Path,
    level_name: &str,
    area_id: u32,
    manifest_out: &Path,
) -> Result<(), Box<dyn std::error::Error>> {
    let level_dir = sm64_root.join("levels").join(level_name);
    if !level_dir.exists() {
        return Err(format!(
            "SM64 level directory not found: {}",
            level_dir.display()
        )
        .into());
    }

    let area_dir = level_dir.join("areas").join(area_id.to_string());
    if !area_dir.exists() {
        return Err(format!(
            "SM64 level area directory not found: {}",
            area_dir.display()
        )
        .into());
    }

    // Collect all sub-area model.inc.c files (areas/{area}/{1}/{model.inc.c}, etc.)
    let mut model_paths: Vec<std::path::PathBuf> = Vec::new();
    let mut sub_id = 1u32;
    loop {
        let candidate = area_dir.join(sub_id.to_string()).join("model.inc.c");
        if candidate.exists() {
            model_paths.push(candidate);
            sub_id += 1;
        } else {
            break;
        }
    }

    if model_paths.is_empty() {
        return Err(format!(
            "No model.inc.c files found under {}: expected files at {}",
            area_dir.display(),
            area_dir.join("1/model.inc.c").display()
        )
        .into());
    }

    // Merge all model files into unified lookup tables
    let mut all_vertex_arrays: HashMap<String, Vec<ParsedVertex>> = HashMap::new();
    let mut all_display_lists: HashMap<String, Vec<ParsedGfxCommand>> = HashMap::new();
    let mut all_light_groups: HashMap<String, ParsedLightGroup> = HashMap::new();
    let mut root_display_list_names: Vec<String> = Vec::new();

    for (model_index, model_path) in model_paths.iter().enumerate() {
        let model_text = fs::read_to_string(model_path).map_err(|err| {
            format!("Failed to read {}: {}", model_path.display(), err)
        })?;

        let vertex_arrays = parse_vertex_arrays(&model_text)?;
        let display_lists = parse_display_lists(&model_text)?;
        let light_groups = parse_light_groups(&model_text)?;

        all_vertex_arrays.extend(vertex_arrays);
        all_light_groups.extend(light_groups);

        // Identify the "root" display list for this sub-area model:
        // it is the last Gfx[] that is not called by any other in this file
        let called_in_this_file: HashSet<String> = display_lists
            .values()
            .flat_map(|cmds| {
                cmds.iter().filter_map(|cmd| {
                    if let ParsedGfxCommand::DisplayList(name) = cmd {
                        Some(name.clone())
                    } else {
                        None
                    }
                })
            })
            .collect();
        let mut local_roots: Vec<String> = display_lists
            .keys()
            .filter(|name| !called_in_this_file.contains(*name))
            .cloned()
            .collect();
        local_roots.sort();

        all_display_lists.extend(display_lists);

        if local_roots.is_empty() {
            // Fallback: use any display list from this model file
            if let Some(first_key) = all_display_lists
                .keys()
                .filter(|k| k.contains(&format!("seg7")))
                .cloned()
                .next()
            {
                root_display_list_names.push(first_key);
            }
        } else {
            root_display_list_names.push(local_roots[0].clone());
        }

        let _ = model_index; // suppress unused warning
    }

    // Convert all parsed display lists to manifest format, scaling positions
    let mut extracted_display_lists: Vec<DisplayListDefinition> = Vec::new();
    for (dl_name, parsed_commands) in &all_display_lists {
        let commands =
            convert_display_list_commands_scaled(parsed_commands, &all_display_lists, &all_vertex_arrays)?;
        extracted_display_lists.push(DisplayListDefinition {
            id: dl_name.clone(),
            commands,
        });
    }
    extracted_display_lists.sort_by(|a, b| a.id.cmp(&b.id));

    // Build the root display list that calls all sub-area roots
    let root_id = format!("{}_area{}_root", level_name, area_id);
    let mut root_commands: Vec<DisplayListCommand> = Vec::new();
    for sub_root in &root_display_list_names {
        if all_display_lists.contains_key(sub_root) {
            root_commands.push(DisplayListCommand::CallDisplayList {
                display_list_id: sub_root.clone(),
            });
        }
    }
    extracted_display_lists.insert(
        0,
        DisplayListDefinition {
            id: root_id.clone(),
            commands: root_commands,
        },
    );

    // Build light groups
    let light_groups: Vec<LightGroupDefinition> = all_light_groups
        .iter()
        .map(|(name, parsed)| LightGroupDefinition {
            id: name.clone(),
            ambient_color: parsed.ambient_color,
            diffuse_color: parsed.diffuse_color,
            direction: parsed.direction,
        })
        .collect();

    // Build placeholder checkerboard textures (one per distinct light group color family)
    let textures: Vec<TextureDefinition> = vec![
        TextureDefinition {
            id: "level_ground".to_string(),
            source: TextureSource::Checkerboard {
                width: 64,
                height: 64,
                cell_size: 8,
                color_a: [100, 160, 70, 255],
                color_b: [80, 135, 55, 255],
            },
        },
        TextureDefinition {
            id: "level_rock".to_string(),
            source: TextureSource::Checkerboard {
                width: 64,
                height: 64,
                cell_size: 16,
                color_a: [160, 140, 110, 255],
                color_b: [130, 115, 85, 255],
            },
        },
    ];

    let manifest = Fast3dSmokeManifest {
        title: format!(
            "SM64 {} Area {} Level Chunk",
            level_name.to_uppercase(),
            area_id
        ),
        root_display_list: root_id,
        resolution: ResolutionConfig {
            width: 1280,
            height: 720,
        },
        clear_color: [100, 160, 220, 255],
        camera: CameraConfig {
            controller_mode: crate::model::CameraControllerMode::Orbit,
            target: [0.0, 5.0, 0.0],
            orbit_radius: 80.0,
            orbit_height: 30.0,
            initial_yaw_radians: 0.0,
            initial_pitch_radians: -0.3,
            fov_y_degrees: 60.0,
            near_plane: 0.5,
            far_plane: 2000.0,
            free_position: [0.0, 20.0, 80.0],
            move_speed: 40.0,
            look_speed: 1.5,
        },
        auto_rotation_radians_per_second: 0.08,
        segment_bindings: Vec::new(),
        light_groups,
        scene_instances: Vec::new(),
        shader_overrides: Vec::new(),
        textures,
        display_lists: extracted_display_lists,
    };

    if let Some(parent) = manifest_out.parent() {
        if !parent.as_os_str().is_empty() {
            fs::create_dir_all(parent)?;
        }
    }
    fs::write(manifest_out, serde_json::to_string_pretty(&manifest)?)?;
    Ok(())
}

/// Convert parsed GFX commands to manifest format, scaling all vertex positions
/// from N64 integer units to world units via `LEVEL_N64_UNIT_SCALE`.
fn convert_display_list_commands_scaled(
    parsed_commands: &[ParsedGfxCommand],
    parsed_display_lists: &HashMap<String, Vec<ParsedGfxCommand>>,
    parsed_vertex_arrays: &HashMap<String, Vec<ParsedVertex>>,
) -> Result<Vec<DisplayListCommand>, String> {
    let mut commands = Vec::new();
    for parsed_command in parsed_commands {
        match parsed_command {
            ParsedGfxCommand::Vertex {
                vertex_array_name,
                count,
                slot,
            } => {
                let parsed_vertices = parsed_vertex_arrays
                    .get(vertex_array_name)
                    .ok_or_else(|| format!("missing vertex array `{vertex_array_name}`"))?;
                let vertices = parsed_vertices
                    .iter()
                    .take(*count)
                    .map(|vertex| Fast3dVertex {
                        position: [
                            vertex.position[0] * LEVEL_N64_UNIT_SCALE,
                            vertex.position[1] * LEVEL_N64_UNIT_SCALE,
                            vertex.position[2] * LEVEL_N64_UNIT_SCALE,
                        ],
                        uv: [
                            vertex.uv_raw[0] / (32.0 * 64.0),
                            vertex.uv_raw[1] / (32.0 * 64.0),
                        ],
                        color: [255, 255, 255, 255],
                        normal: Some(vertex.normal),
                    })
                    .collect::<Vec<_>>();
                commands.push(DisplayListCommand::LoadVertices {
                    slot: *slot,
                    vertices,
                });
            }
            ParsedGfxCommand::TwoTriangles(left, right) => {
                commands.push(DisplayListCommand::DrawTriangles {
                    triangles: vec![*left, *right],
                });
            }
            ParsedGfxCommand::OneTriangle(triangle) => {
                commands.push(DisplayListCommand::DrawTriangles {
                    triangles: vec![*triangle],
                });
            }
            ParsedGfxCommand::DisplayList(display_list_name) => {
                if !parsed_display_lists.contains_key(display_list_name) {
                    // Skip orphaned display list references — level data often references
                    // display lists from other segments that are not in this source file
                    continue;
                }
                commands.push(DisplayListCommand::CallDisplayList {
                    display_list_id: display_list_name.clone(),
                });
            }
            ParsedGfxCommand::SetTexture(texture_name) => {
                commands.push(DisplayListCommand::BindTexture {
                    texture_id: texture_name.clone(),
                });
            }
            ParsedGfxCommand::SetCombineMode(mode) => {
                commands.push(DisplayListCommand::SetCombineMode { mode: *mode });
            }
            ParsedGfxCommand::SetLightGroup(light_group_name) => {
                commands.push(DisplayListCommand::SetLightGroup {
                    light_group_id: light_group_name.clone(),
                });
            }
        }
    }
    Ok(commands)
}


#[derive(Clone, Copy, Debug)]
struct ParsedVertex {
    position: [f32; 3],
    uv_raw: [f32; 2],
    normal: [f32; 3],
}

#[derive(Clone, Debug)]
enum ParsedGfxCommand {
    Vertex {
        vertex_array_name: String,
        count: usize,
        slot: u16,
    },
    TwoTriangles([u16; 3], [u16; 3]),
    OneTriangle([u16; 3]),
    DisplayList(String),
    SetTexture(String),
    SetCombineMode(CombineMode),
    SetLightGroup(String),
}

#[derive(Clone, Copy, Debug)]
struct ParsedLightGroup {
    ambient_color: [u8; 4],
    diffuse_color: [u8; 4],
    direction: [f32; 3],
}

pub fn extract_sm64_title_face_scene(
    sm64_root: &Path,
    manifest_out: &Path,
) -> Result<(), Box<dyn std::error::Error>> {
    let mario_model_text = fs::read_to_string(sm64_root.join(MARIO_FACE_MODEL_PATH))?;
    let parsed_light_groups = parse_light_groups(&mario_model_text)?;
    let parsed_vertex_arrays = parse_vertex_arrays(&mario_model_text)?;
    let parsed_display_lists = parse_display_lists(&mario_model_text)?;

    let root_display_list_name = "mario_cap_off_eyes_front";
    let title_face_matrix = build_title_face_matrix(
        root_display_list_name,
        &parsed_display_lists,
        &parsed_vertex_arrays,
    )?;
    let mut visited_display_lists = HashSet::new();
    collect_display_list_closure(
        root_display_list_name,
        &parsed_display_lists,
        &mut visited_display_lists,
    )?;

    let mut extracted_display_lists = Vec::new();
    for display_list_name in visited_display_lists.iter() {
        let parsed_commands = parsed_display_lists
            .get(display_list_name)
            .ok_or_else(|| format!("missing parsed display list `{display_list_name}`"))?;
        extracted_display_lists.push(DisplayListDefinition {
            id: display_list_name.clone(),
            commands: convert_display_list_commands(
                parsed_commands,
                &parsed_display_lists,
                &parsed_vertex_arrays,
            )?,
        });
    }
    extracted_display_lists.sort_by(|left, right| left.id.cmp(&right.id));
    if let Some(root_display_list) = extracted_display_lists
        .iter_mut()
        .find(|display_list| display_list.id == root_display_list_name)
    {
        root_display_list.commands = retarget_title_face_root_commands(&root_display_list.commands);
    }

    let mut required_light_groups = BTreeMap::new();
    for display_list in &extracted_display_lists {
        for command in &display_list.commands {
            if let DisplayListCommand::SetLightGroup { light_group_id } = command {
                required_light_groups.insert(light_group_id.clone(), ());
            }
        }
    }
    let light_groups = required_light_groups
        .keys()
        .map(|light_group_id| {
            let parsed = parsed_light_groups
                .get(light_group_id)
                .ok_or_else(|| format!("missing parsed light group `{light_group_id}`"))?;
            Ok(LightGroupDefinition {
                id: light_group_id.clone(),
                ambient_color: parsed.ambient_color,
                diffuse_color: parsed.diffuse_color,
                direction: parsed.direction,
            })
        })
        .collect::<Result<Vec<_>, String>>()?;

    let mut display_lists = vec![
        DisplayListDefinition {
            id: "title_backdrop_quad".to_string(),
            commands: vec![
                DisplayListCommand::LoadVertices {
                    slot: 0,
                    vertices: vec![
                        Fast3dVertex {
                            position: [-5.6, -3.2, -8.0],
                            uv: [0.0, 1.0],
                            color: [255, 255, 255, 255],
                            normal: None,
                        },
                        Fast3dVertex {
                            position: [5.6, -3.2, -8.0],
                            uv: [1.0, 1.0],
                            color: [255, 255, 255, 255],
                            normal: None,
                        },
                        Fast3dVertex {
                            position: [5.6, 3.2, -8.0],
                            uv: [1.0, 0.0],
                            color: [255, 255, 255, 255],
                            normal: None,
                        },
                        Fast3dVertex {
                            position: [-5.6, 3.2, -8.0],
                            uv: [0.0, 0.0],
                            color: [255, 255, 255, 255],
                            normal: None,
                        },
                    ],
                },
                DisplayListCommand::DrawTriangles {
                    triangles: vec![[0, 1, 2], [0, 2, 3]],
                },
            ],
        },
        DisplayListDefinition {
            id: "title_root".to_string(),
            commands: vec![
                DisplayListCommand::BindTexture {
                    texture_id: "sm64_title_card".to_string(),
                },
                DisplayListCommand::SetCombineMode {
                    mode: CombineMode::Texture,
                },
                DisplayListCommand::CallDisplayList {
                    display_list_id: "title_backdrop_quad".to_string(),
                },
                DisplayListCommand::SetCombineMode {
                    mode: CombineMode::TextureVertex,
                },
                DisplayListCommand::PushMatrix {
                    matrix: title_face_matrix,
                },
                DisplayListCommand::CallDisplayList {
                    display_list_id: root_display_list_name.to_string(),
                },
                DisplayListCommand::PopMatrix,
            ],
        },
    ];
    display_lists.extend(extracted_display_lists);

    let manifest = Fast3dSmokeManifest {
        title: "SM64 Title Face Extraction Smoke".to_string(),
        root_display_list: "title_root".to_string(),
        resolution: ResolutionConfig {
            width: 960,
            height: 720,
        },
        clear_color: [8, 12, 24, 255],
        camera: CameraConfig {
            controller_mode: crate::model::CameraControllerMode::Orbit,
            target: [0.9, -0.05, 0.0],
            orbit_radius: 5.0,
            orbit_height: 0.45,
            initial_yaw_radians: std::f32::consts::FRAC_PI_2,
            initial_pitch_radians: -0.06,
            fov_y_degrees: 34.0,
            near_plane: 0.1,
            far_plane: 80.0,
            free_position: [0.0, 2.0, 5.0],
            move_speed: 5.5,
            look_speed: 1.35,
        },
        auto_rotation_radians_per_second: 0.0,
        segment_bindings: Vec::new(),
        light_groups,
        scene_instances: Vec::new(),
        shader_overrides: Vec::new(),
        textures: vec![
            TextureDefinition {
                id: "sm64_title_card".to_string(),
                source: TextureSource::GeneratedSm64TitleCard {
                    width: 1024,
                    height: 512,
                },
            },
            TextureDefinition {
                id: "mario_eyes_front".to_string(),
                source: TextureSource::GeneratedMarioEyesFront {
                    width: 32,
                    height: 32,
                },
            },
            TextureDefinition {
                id: "mario_mustache".to_string(),
                source: TextureSource::GeneratedMarioMustache {
                    width: 32,
                    height: 32,
                },
            },
            TextureDefinition {
                id: "mario_sideburn".to_string(),
                source: TextureSource::GeneratedMarioSideburn {
                    width: 32,
                    height: 32,
                },
            },
        ],
        display_lists,
    };

    if let Some(parent) = manifest_out.parent() {
        if !parent.as_os_str().is_empty() {
            fs::create_dir_all(parent)?;
        }
    }
    fs::write(manifest_out, serde_json::to_string_pretty(&manifest)?)?;
    Ok(())
}

fn build_title_face_matrix(
    root_display_list_name: &str,
    parsed_display_lists: &HashMap<String, Vec<ParsedGfxCommand>>,
    parsed_vertex_arrays: &HashMap<String, Vec<ParsedVertex>>,
) -> Result<[[f32; 4]; 4], String> {
    let face_positions = collect_display_list_positions(
        root_display_list_name,
        parsed_display_lists,
        parsed_vertex_arrays,
    )?;
    let rotation = Mat4::from_rotation_y(TITLE_FACE_ROTATION_Y_RADIANS);
    let mut minimum = Vec3::splat(f32::INFINITY);
    let mut maximum = Vec3::splat(f32::NEG_INFINITY);
    for position in face_positions {
        let rotated = rotation.transform_point3(position);
        minimum = minimum.min(rotated);
        maximum = maximum.max(rotated);
    }

    let extents = (maximum - minimum).max(Vec3::splat(1.0));
    let center = (minimum + maximum) * 0.5;
    let uniform_scale =
        (TITLE_FACE_TARGET_WIDTH / extents.x).min(TITLE_FACE_TARGET_HEIGHT / extents.y);
    let desired_center = Vec3::from_array(TITLE_FACE_TARGET_CENTER);
    let translation = desired_center - center * uniform_scale;
    let transform = Mat4::from_translation(translation)
        * Mat4::from_scale(Vec3::splat(uniform_scale))
        * rotation;
    Ok(rows_from_glam_matrix(transform))
}

fn collect_display_list_positions(
    display_list_name: &str,
    parsed_display_lists: &HashMap<String, Vec<ParsedGfxCommand>>,
    parsed_vertex_arrays: &HashMap<String, Vec<ParsedVertex>>,
) -> Result<Vec<Vec3>, String> {
    let mut visited = HashSet::new();
    let mut positions = Vec::new();
    collect_display_list_positions_recursive(
        display_list_name,
        parsed_display_lists,
        parsed_vertex_arrays,
        &mut visited,
        &mut positions,
    )?;
    if positions.is_empty() {
        return Err(format!(
            "display list `{display_list_name}` did not reference any vertex positions"
        ));
    }
    Ok(positions)
}

fn collect_display_list_positions_recursive(
    display_list_name: &str,
    parsed_display_lists: &HashMap<String, Vec<ParsedGfxCommand>>,
    parsed_vertex_arrays: &HashMap<String, Vec<ParsedVertex>>,
    visited: &mut HashSet<String>,
    positions: &mut Vec<Vec3>,
) -> Result<(), String> {
    if !visited.insert(display_list_name.to_string()) {
        return Ok(());
    }
    let commands = parsed_display_lists
        .get(display_list_name)
        .ok_or_else(|| format!("missing display list `{display_list_name}`"))?;
    for command in commands {
        match command {
            ParsedGfxCommand::Vertex {
                vertex_array_name,
                count,
                ..
            } => {
                let vertices = parsed_vertex_arrays
                    .get(vertex_array_name)
                    .ok_or_else(|| format!("missing vertex array `{vertex_array_name}`"))?;
                for vertex in vertices.iter().take(*count) {
                    positions.push(Vec3::from_array(vertex.position));
                }
            }
            ParsedGfxCommand::DisplayList(child_name) => {
                collect_display_list_positions_recursive(
                    child_name,
                    parsed_display_lists,
                    parsed_vertex_arrays,
                    visited,
                    positions,
                )?;
            }
            ParsedGfxCommand::TwoTriangles(_, _)
            | ParsedGfxCommand::OneTriangle(_)
            | ParsedGfxCommand::SetTexture(_)
            | ParsedGfxCommand::SetCombineMode(_)
            | ParsedGfxCommand::SetLightGroup(_) => {}
        }
    }
    Ok(())
}

fn rows_from_glam_matrix(matrix: Mat4) -> [[f32; 4]; 4] {
    matrix.transpose().to_cols_array_2d()
}

fn retarget_title_face_root_commands(commands: &[DisplayListCommand]) -> Vec<DisplayListCommand> {
    let mut rewritten = Vec::with_capacity(commands.len() + 2);
    for command in commands {
        match command {
            DisplayListCommand::CallDisplayList { display_list_id }
                if display_list_id == "mario_face_cap_off_dl" =>
            {
                rewritten.push(DisplayListCommand::SetCombineMode {
                    mode: CombineMode::Vertex,
                });
                rewritten.push(DisplayListCommand::CallDisplayList {
                    display_list_id: "mario_face_part_cap_off_dl".to_string(),
                });
            }
            _ => rewritten.push(command.clone()),
        }
    }
    rewritten
}

fn collect_display_list_closure(
    display_list_name: &str,
    parsed_display_lists: &HashMap<String, Vec<ParsedGfxCommand>>,
    visited: &mut HashSet<String>,
) -> Result<(), String> {
    if !visited.insert(display_list_name.to_string()) {
        return Ok(());
    }
    let commands = parsed_display_lists
        .get(display_list_name)
        .ok_or_else(|| format!("missing display list `{display_list_name}`"))?;
    for command in commands {
        if let ParsedGfxCommand::DisplayList(child_name) = command {
            collect_display_list_closure(child_name, parsed_display_lists, visited)?;
        }
    }
    Ok(())
}

fn convert_display_list_commands(
    parsed_commands: &[ParsedGfxCommand],
    parsed_display_lists: &HashMap<String, Vec<ParsedGfxCommand>>,
    parsed_vertex_arrays: &HashMap<String, Vec<ParsedVertex>>,
) -> Result<Vec<DisplayListCommand>, String> {
    let mut commands = Vec::new();
    for parsed_command in parsed_commands {
        match parsed_command {
            ParsedGfxCommand::Vertex {
                vertex_array_name,
                count,
                slot,
            } => {
                let parsed_vertices = parsed_vertex_arrays
                    .get(vertex_array_name)
                    .ok_or_else(|| format!("missing vertex array `{vertex_array_name}`"))?;
                let vertices = parsed_vertices
                    .iter()
                    .take(*count)
                    .map(|vertex| Fast3dVertex {
                        position: vertex.position,
                        uv: [
                            vertex.uv_raw[0] / (32.0 * 32.0),
                            vertex.uv_raw[1] / (32.0 * 32.0),
                        ],
                        color: [255, 255, 255, 255],
                        normal: Some(vertex.normal),
                    })
                    .collect::<Vec<_>>();
                commands.push(DisplayListCommand::LoadVertices {
                    slot: *slot,
                    vertices,
                });
            }
            ParsedGfxCommand::TwoTriangles(left, right) => {
                commands.push(DisplayListCommand::DrawTriangles {
                    triangles: vec![*left, *right],
                });
            }
            ParsedGfxCommand::OneTriangle(triangle) => {
                commands.push(DisplayListCommand::DrawTriangles {
                    triangles: vec![*triangle],
                });
            }
            ParsedGfxCommand::DisplayList(display_list_name) => {
                if !parsed_display_lists.contains_key(display_list_name) {
                    return Err(format!("display list `{display_list_name}` missing"));
                }
                commands.push(DisplayListCommand::CallDisplayList {
                    display_list_id: display_list_name.clone(),
                });
            }
            ParsedGfxCommand::SetTexture(texture_name) => {
                commands.push(DisplayListCommand::BindTexture {
                    texture_id: texture_name.clone(),
                });
            }
            ParsedGfxCommand::SetCombineMode(mode) => {
                commands.push(DisplayListCommand::SetCombineMode { mode: *mode });
            }
            ParsedGfxCommand::SetLightGroup(light_group_name) => {
                commands.push(DisplayListCommand::SetLightGroup {
                    light_group_id: light_group_name.clone(),
                });
            }
        }
    }
    Ok(commands)
}

fn parse_light_groups(input: &str) -> Result<HashMap<String, ParsedLightGroup>, String> {
    let light_regex = Regex::new(
        r"static const Lights1 (?P<name>[A-Za-z0-9_]+)\s*=\s*gdSPDefLights1\(\s*(?P<a0>0x[0-9a-fA-F]+|\d+),\s*(?P<a1>0x[0-9a-fA-F]+|\d+),\s*(?P<a2>0x[0-9a-fA-F]+|\d+),\s*(?P<d0>0x[0-9a-fA-F]+|\d+),\s*(?P<d1>0x[0-9a-fA-F]+|\d+),\s*(?P<d2>0x[0-9a-fA-F]+|\d+),\s*(?P<x>0x[0-9a-fA-F]+|\d+),\s*(?P<y>0x[0-9a-fA-F]+|\d+),\s*(?P<z>0x[0-9a-fA-F]+|\d+)\s*\);",
    )
    .map_err(|error| format!("failed to compile light regex: {error}"))?;

    let mut lights = HashMap::new();
    for capture in light_regex.captures_iter(input) {
        lights.insert(
            capture["name"].to_string(),
            ParsedLightGroup {
                ambient_color: [
                    parse_u8_literal(&capture["a0"])?,
                    parse_u8_literal(&capture["a1"])?,
                    parse_u8_literal(&capture["a2"])?,
                    255,
                ],
                diffuse_color: [
                    parse_u8_literal(&capture["d0"])?,
                    parse_u8_literal(&capture["d1"])?,
                    parse_u8_literal(&capture["d2"])?,
                    255,
                ],
                direction: [
                    parse_signed_component(&capture["x"])?,
                    parse_signed_component(&capture["y"])?,
                    parse_signed_component(&capture["z"])?,
                ],
            },
        );
    }
    Ok(lights)
}

fn parse_vertex_arrays(input: &str) -> Result<HashMap<String, Vec<ParsedVertex>>, String> {
    let array_start_regex = Regex::new(r"static const Vtx (?P<name>[A-Za-z0-9_]+)\[\]\s*=\s*\{")
        .map_err(|error| format!("failed to compile vertex start regex: {error}"))?;
    let vertex_line_regex = Regex::new(
        r"\{\{\{\s*(?P<x>-?\d+),\s*(?P<y>-?\d+),\s*(?P<z>-?\d+)\},\s*0,\s*\{\s*(?P<u>-?\d+),\s*(?P<v>-?\d+)\},\s*\{(?P<nx>0x[0-9a-fA-F]+|-?\d+),\s*(?P<ny>0x[0-9a-fA-F]+|-?\d+),\s*(?P<nz>0x[0-9a-fA-F]+|-?\d+),\s*(?P<na>0x[0-9a-fA-F]+|-?\d+)\}\}\}",
    )
    .map_err(|error| format!("failed to compile vertex regex: {error}"))?;

    let mut arrays = HashMap::new();
    let mut current_name: Option<String> = None;
    let mut current_vertices = Vec::new();

    for line in input.lines() {
        if current_name.is_none() {
            if let Some(capture) = array_start_regex.captures(line) {
                current_name = Some(capture["name"].to_string());
                current_vertices.clear();
            }
            continue;
        }

        if line.trim_start().starts_with("};") {
            if let Some(name) = current_name.take() {
                arrays.insert(name, current_vertices.clone());
            }
            current_vertices.clear();
            continue;
        }

        if let Some(capture) = vertex_line_regex.captures(line) {
            current_vertices.push(ParsedVertex {
                position: [
                    capture["x"].parse::<f32>().unwrap_or(0.0),
                    capture["y"].parse::<f32>().unwrap_or(0.0),
                    capture["z"].parse::<f32>().unwrap_or(0.0),
                ],
                uv_raw: [
                    capture["u"].parse::<f32>().unwrap_or(0.0),
                    capture["v"].parse::<f32>().unwrap_or(0.0),
                ],
                normal: [
                    parse_signed_component(&capture["nx"])?,
                    parse_signed_component(&capture["ny"])?,
                    parse_signed_component(&capture["nz"])?,
                ],
            });
        }
    }

    Ok(arrays)
}

fn parse_display_lists(input: &str) -> Result<HashMap<String, Vec<ParsedGfxCommand>>, String> {
    let display_list_start_regex = Regex::new(r"const Gfx (?P<name>[A-Za-z0-9_]+)\[\]\s*=\s*\{")
        .map_err(|error| format!("failed to compile display-list start regex: {error}"))?;
    let vertex_regex =
        Regex::new(r"gsSPVertex\((?P<name>[A-Za-z0-9_]+),\s*(?P<count>\d+),\s*(?P<slot>\d+)\)")
            .map_err(|error| format!("failed to compile gsSPVertex regex: {error}"))?;
    let tri2_regex = Regex::new(
        r"gsSP2Triangles\(\s*(?P<a0>\d+),\s*(?P<a1>\d+),\s*(?P<a2>\d+),\s*0x0,\s*(?P<b0>\d+),\s*(?P<b1>\d+),\s*(?P<b2>\d+),\s*0x0\)",
    )
    .map_err(|error| format!("failed to compile gsSP2Triangles regex: {error}"))?;
    let tri1_regex =
        Regex::new(r"gsSP1Triangle\(\s*(?P<a0>\d+),\s*(?P<a1>\d+),\s*(?P<a2>\d+),\s*0x0\)")
            .map_err(|error| format!("failed to compile gsSP1Triangle regex: {error}"))?;
    let call_regex = Regex::new(r"gsSPDisplayList\((?P<name>[A-Za-z0-9_]+)\)")
        .map_err(|error| format!("failed to compile gsSPDisplayList regex: {error}"))?;
    let texture_regex =
        Regex::new(r"gsDPSetTextureImage\([^,]+,\s*[^,]+,\s*[^,]+,\s*(?P<name>[A-Za-z0-9_]+)\)")
            .map_err(|error| format!("failed to compile texture regex: {error}"))?;
    let combine_mode_regex =
        Regex::new(r"gsDPSetCombineMode\((?P<name>[A-Za-z0-9_]+),\s*[A-Za-z0-9_]+\)")
            .map_err(|error| format!("failed to compile combine regex: {error}"))?;
    let light_regex = Regex::new(r"gsSPLight\(&(?P<name>[A-Za-z0-9_]+)\.(?:l|a),\s*[12]\)")
        .map_err(|error| format!("failed to compile light regex: {error}"))?;

    let mut display_lists = HashMap::new();
    let mut current_name: Option<String> = None;
    let mut current_commands = Vec::new();

    for line in input.lines() {
        if current_name.is_none() {
            if let Some(capture) = display_list_start_regex.captures(line) {
                current_name = Some(capture["name"].to_string());
                current_commands.clear();
            }
            continue;
        }

        if line.trim_start().starts_with("};") {
            if let Some(name) = current_name.take() {
                display_lists.insert(name, current_commands.clone());
            }
            current_commands.clear();
            continue;
        }

        if let Some(capture) = vertex_regex.captures(line) {
            current_commands.push(ParsedGfxCommand::Vertex {
                vertex_array_name: capture["name"].to_string(),
                count: capture["count"].parse::<usize>().unwrap_or(0),
                slot: capture["slot"].parse::<u16>().unwrap_or(0),
            });
            continue;
        }
        if let Some(capture) = tri2_regex.captures(line) {
            current_commands.push(ParsedGfxCommand::TwoTriangles(
                [
                    capture["a0"].parse::<u16>().unwrap_or(0),
                    capture["a1"].parse::<u16>().unwrap_or(0),
                    capture["a2"].parse::<u16>().unwrap_or(0),
                ],
                [
                    capture["b0"].parse::<u16>().unwrap_or(0),
                    capture["b1"].parse::<u16>().unwrap_or(0),
                    capture["b2"].parse::<u16>().unwrap_or(0),
                ],
            ));
            continue;
        }
        if let Some(capture) = tri1_regex.captures(line) {
            current_commands.push(ParsedGfxCommand::OneTriangle([
                capture["a0"].parse::<u16>().unwrap_or(0),
                capture["a1"].parse::<u16>().unwrap_or(0),
                capture["a2"].parse::<u16>().unwrap_or(0),
            ]));
            continue;
        }
        if let Some(capture) = call_regex.captures(line) {
            current_commands.push(ParsedGfxCommand::DisplayList(capture["name"].to_string()));
            continue;
        }
        if let Some(capture) = texture_regex.captures(line) {
            if let Some(texture_name) = map_mario_texture_name(&capture["name"]) {
                current_commands.push(ParsedGfxCommand::SetTexture(texture_name.to_string()));
            }
            continue;
        }
        if let Some(capture) = combine_mode_regex.captures(line) {
            if let Some(mode) = map_sm64_combine_mode(&capture["name"]) {
                current_commands.push(ParsedGfxCommand::SetCombineMode(mode));
            }
            continue;
        }
        if let Some(capture) = light_regex.captures(line) {
            current_commands.push(ParsedGfxCommand::SetLightGroup(capture["name"].to_string()));
        }
    }

    Ok(display_lists)
}

fn map_mario_texture_name(name: &str) -> Option<&'static str> {
    match name {
        "mario_texture_eyes_front" => Some("mario_eyes_front"),
        "mario_texture_mustache" => Some("mario_mustache"),
        "mario_texture_hair_sideburn" => Some("mario_sideburn"),
        _ => None,
    }
}

fn map_sm64_combine_mode(name: &str) -> Option<CombineMode> {
    match name {
        "G_CC_BLENDRGBFADEA" => Some(CombineMode::TextureVertex),
        "G_CC_DECALFADE" => Some(CombineMode::Texture),
        "G_CC_SHADEFADEA" | "G_CC_SHADE" => Some(CombineMode::Vertex),
        _ => None,
    }
}

fn parse_u8_literal(value: &str) -> Result<u8, String> {
    let parsed = if let Some(stripped) = value.strip_prefix("0x") {
        u16::from_str_radix(stripped, 16)
            .map_err(|error| format!("invalid hex literal: {error}"))?
    } else {
        value
            .parse::<u16>()
            .map_err(|error| format!("invalid decimal literal: {error}"))?
    };
    Ok(parsed as u8)
}

fn parse_signed_component(value: &str) -> Result<f32, String> {
    let raw = parse_u8_literal(value)?;
    Ok((raw as i8) as f32 / 127.0)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parses_signed_hex_component() {
        assert!(parse_signed_component("0x7f").unwrap() > 0.99);
        assert!(parse_signed_component("0x80").unwrap() < -0.99);
    }

    #[test]
    fn maps_supported_mario_textures() {
        assert_eq!(
            map_mario_texture_name("mario_texture_eyes_front"),
            Some("mario_eyes_front")
        );
        assert_eq!(
            map_mario_texture_name("mario_texture_mustache"),
            Some("mario_mustache")
        );
        assert_eq!(
            map_mario_texture_name("mario_texture_hair_sideburn"),
            Some("mario_sideburn")
        );
        assert_eq!(map_mario_texture_name("mario_texture_m_logo"), None);
    }

    #[test]
    fn maps_supported_sm64_combine_modes() {
        assert_eq!(
            map_sm64_combine_mode("G_CC_BLENDRGBFADEA"),
            Some(CombineMode::TextureVertex)
        );
        assert_eq!(
            map_sm64_combine_mode("G_CC_SHADEFADEA"),
            Some(CombineMode::Vertex)
        );
    }
}
