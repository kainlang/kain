use std::{collections::HashMap, fs, path::Path};

use glam::Vec4Swizzles;

use crate::combiner::CombinerState;
use crate::math::{
    matrix_from_rows, orbit_camera_position, vec2_from_array, vec3_from_array, vec4_from_rgba8,
    Float4, Matrix4,
};
use crate::model::{
    CombineMode, DisplayListCommand, DisplayListDefinition, Fast3dSmokeManifest, Fast3dVertex,
    SegmentBindingKind,
};
use crate::rasterizer::{Framebuffer, RenderFrame, RenderStats, ScreenVertex};
use crate::texture::{build_texture_catalog, TextureImage};

const FAST3D_VERTEX_CACHE_CAPACITY: usize = 64;

#[derive(Clone, Copy, Debug)]
pub struct OrbitControls {
    pub yaw_radians: f32,
    pub pitch_radians: f32,
    pub zoom_delta: f32,
}

impl Default for OrbitControls {
    fn default() -> Self {
        Self {
            yaw_radians: 0.0,
            pitch_radians: 0.15,
            zoom_delta: 0.0,
        }
    }
}

#[derive(Clone, Debug)]
pub struct Fast3dRuntime {
    pub manifest: Fast3dSmokeManifest,
    display_lists_by_id: HashMap<String, DisplayListDefinition>,
    textures_by_id: HashMap<String, TextureImage>,
    texture_segments: HashMap<u8, String>,
    display_list_segments: HashMap<u8, String>,
}

impl Fast3dRuntime {
    pub fn load_from_path(path: &Path) -> Result<Self, Box<dyn std::error::Error>> {
        let manifest_text = fs::read_to_string(path)?;
        let manifest: Fast3dSmokeManifest = serde_json::from_str(&manifest_text)?;
        let display_lists_by_id = manifest
            .display_lists
            .iter()
            .map(|display_list| (display_list.id.clone(), display_list.clone()))
            .collect::<HashMap<_, _>>();
        let textures_by_id = build_texture_catalog(&manifest.textures)?;

        let mut texture_segments = HashMap::new();
        let mut display_list_segments = HashMap::new();
        for binding in &manifest.segment_bindings {
            match binding.kind {
                SegmentBindingKind::Texture => {
                    texture_segments.insert(binding.segment_id, binding.target_id.clone());
                }
                SegmentBindingKind::DisplayList => {
                    display_list_segments.insert(binding.segment_id, binding.target_id.clone());
                }
            }
        }

        Ok(Self {
            manifest,
            display_lists_by_id,
            textures_by_id,
            texture_segments,
            display_list_segments,
        })
    }

    pub fn render_frame(
        &self,
        time_seconds: f32,
        orbit_controls: &OrbitControls,
        combine_override: Option<CombineMode>,
    ) -> Result<RenderFrame, String> {
        let resolution = self.manifest.resolution;
        let mut framebuffer =
            Framebuffer::new(resolution.width, resolution.height, self.manifest.clear_color);
        let mut stats = RenderStats::default();
        let view_projection = self.build_view_projection(time_seconds, orbit_controls);
        let mut execution = ExecutionState::new();
        self.execute_display_list(
            &self.manifest.root_display_list,
            &view_projection,
            combine_override,
            &mut execution,
            &mut framebuffer,
            &mut stats,
        )?;
        Ok(framebuffer.finish(stats))
    }

    fn build_view_projection(
        &self,
        time_seconds: f32,
        orbit_controls: &OrbitControls,
    ) -> Matrix4 {
        let camera = self.manifest.camera;
        let yaw = time_seconds * self.manifest.auto_rotation_radians_per_second
            + orbit_controls.yaw_radians;
        let radius = (camera.orbit_radius + orbit_controls.zoom_delta).max(1.25);
        let target = vec3_from_array(camera.target);
        let position = orbit_camera_position(
            target,
            radius,
            camera.orbit_height,
            yaw,
            orbit_controls.pitch_radians,
        );
        let view = Matrix4::look_at_rh(position, target, glam::Vec3::Y);
        let aspect_ratio =
            self.manifest.resolution.width as f32 / self.manifest.resolution.height as f32;
        let projection = Matrix4::perspective_rh(
            camera.fov_y_degrees.to_radians(),
            aspect_ratio,
            camera.near_plane,
            camera.far_plane,
        );
        projection * view
    }

    fn execute_display_list(
        &self,
        display_list_id: &str,
        view_projection: &Matrix4,
        combine_override: Option<CombineMode>,
        execution: &mut ExecutionState,
        framebuffer: &mut Framebuffer,
        stats: &mut RenderStats,
    ) -> Result<(), String> {
        let display_list = self
            .display_lists_by_id
            .get(display_list_id)
            .ok_or_else(|| format!("missing display list `{display_list_id}`"))?;

        for command in &display_list.commands {
            match command {
                DisplayListCommand::PushMatrix { matrix } => {
                    let local = matrix_from_rows(*matrix);
                    let parent = *execution
                        .matrix_stack
                        .last()
                        .ok_or("matrix stack unexpectedly empty")?;
                    execution.matrix_stack.push(parent * local);
                }
                DisplayListCommand::PopMatrix => {
                    if execution.matrix_stack.len() > 1 {
                        execution.matrix_stack.pop();
                    }
                }
                DisplayListCommand::LoadVertices { slot, vertices } => {
                    execution.load_vertices(*slot, vertices)?;
                }
                DisplayListCommand::DrawTriangles { triangles } => {
                    for triangle in triangles {
                        let loaded = triangle.map(|index| {
                            execution
                                .vertex_cache
                                .get(index as usize)
                                .and_then(|vertex| *vertex)
                                .ok_or_else(|| format!("vertex slot {index} is not loaded"))
                        });
                        let [left, middle, right] = loaded;
                        let left = left?;
                        let middle = middle?;
                        let right = right?;
                        let current_model = *execution
                            .matrix_stack
                            .last()
                            .ok_or("matrix stack unexpectedly empty")?;
                        let screen_vertices = [
                            self.project_vertex(left, current_model, view_projection, framebuffer)?,
                            self.project_vertex(middle, current_model, view_projection, framebuffer)?,
                            self.project_vertex(right, current_model, view_projection, framebuffer)?,
                        ];
                        let texture = execution
                            .bound_texture
                            .as_ref()
                            .and_then(|texture_id| self.textures_by_id.get(texture_id));
                        let combine_mode = combine_override.unwrap_or(execution.combine_mode);
                        framebuffer.draw_triangle(
                            screen_vertices,
                            texture,
                            CombinerState {
                                mode: combine_mode,
                                primitive_color: execution.primitive_color,
                                env_color: execution.env_color,
                            },
                            stats,
                        );
                    }
                }
                DisplayListCommand::BindTexture { texture_id } => {
                    if !self.textures_by_id.contains_key(texture_id) {
                        return Err(format!("missing texture `{texture_id}`"));
                    }
                    execution.bound_texture = Some(texture_id.clone());
                }
                DisplayListCommand::BindTextureSegment { segment_id } => {
                    let texture_id = self
                        .texture_segments
                        .get(segment_id)
                        .ok_or_else(|| format!("missing texture segment {segment_id}"))?;
                    execution.bound_texture = Some(texture_id.clone());
                }
                DisplayListCommand::CallDisplayList { display_list_id } => {
                    self.execute_display_list(
                        display_list_id,
                        view_projection,
                        combine_override,
                        execution,
                        framebuffer,
                        stats,
                    )?;
                }
                DisplayListCommand::CallDisplayListSegment { segment_id } => {
                    let target = self
                        .display_list_segments
                        .get(segment_id)
                        .ok_or_else(|| format!("missing display-list segment {segment_id}"))?
                        .clone();
                    self.execute_display_list(
                        &target,
                        view_projection,
                        combine_override,
                        execution,
                        framebuffer,
                        stats,
                    )?;
                }
                DisplayListCommand::SetCombineMode { mode } => {
                    execution.combine_mode = *mode;
                }
                DisplayListCommand::SetPrimitiveColor { color } => {
                    execution.primitive_color = vec4_from_rgba8(*color);
                }
                DisplayListCommand::SetEnvColor { color } => {
                    execution.env_color = vec4_from_rgba8(*color);
                }
            }
        }
        Ok(())
    }

    fn project_vertex(
        &self,
        vertex: LoadedVertex,
        model_matrix: Matrix4,
        view_projection: &Matrix4,
        framebuffer: &Framebuffer,
    ) -> Result<ScreenVertex, String> {
        let world = model_matrix * vertex.position.extend(1.0);
        let clip = *view_projection * world;
        if clip.w.abs() <= f32::EPSILON {
            return Err("encountered clip-space vertex with zero w".to_string());
        }
        let ndc = clip.xyz() / clip.w;
        let x = (ndc.x * 0.5 + 0.5) * framebuffer.width as f32;
        let y = (1.0 - (ndc.y * 0.5 + 0.5)) * framebuffer.height as f32;
        let inv_w = 1.0 / clip.w;
        Ok(ScreenVertex {
            x,
            y,
            depth: ndc.z,
            inv_w,
            uv_over_w: vertex.uv * inv_w,
            color_over_w: vertex.color * inv_w,
        })
    }
}

#[derive(Clone, Copy, Debug)]
struct LoadedVertex {
    position: glam::Vec3,
    uv: glam::Vec2,
    color: Float4,
}

#[derive(Clone, Debug)]
struct ExecutionState {
    matrix_stack: Vec<Matrix4>,
    vertex_cache: Vec<Option<LoadedVertex>>,
    bound_texture: Option<String>,
    combine_mode: CombineMode,
    primitive_color: Float4,
    env_color: Float4,
}

impl ExecutionState {
    fn new() -> Self {
        Self {
            matrix_stack: vec![Matrix4::IDENTITY],
            vertex_cache: vec![None; FAST3D_VERTEX_CACHE_CAPACITY],
            bound_texture: None,
            combine_mode: CombineMode::TextureVertex,
            primitive_color: vec4_from_rgba8([255, 255, 255, 255]),
            env_color: vec4_from_rgba8([255, 255, 255, 255]),
        }
    }

    fn load_vertices(&mut self, slot: u16, vertices: &[Fast3dVertex]) -> Result<(), String> {
        let start = slot as usize;
        let end = start + vertices.len();
        if end > self.vertex_cache.len() {
            return Err(format!(
                "vertex load [{start}..{end}) exceeds cache capacity {}",
                self.vertex_cache.len()
            ));
        }
        for (index, vertex) in vertices.iter().enumerate() {
            self.vertex_cache[start + index] = Some(LoadedVertex {
                position: vec3_from_array(vertex.position),
                uv: vec2_from_array(vertex.uv),
                color: vec4_from_rgba8(vertex.color),
            });
        }
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use std::path::PathBuf;

    use super::*;

    fn fixture_manifest_path() -> PathBuf {
        PathBuf::from(env!("CARGO_MANIFEST_DIR"))
            .join("..")
            .join("scene_manifest.json")
    }

    #[test]
    fn manifest_loads_and_renders() {
        let runtime = Fast3dRuntime::load_from_path(&fixture_manifest_path()).unwrap();
        let frame = runtime
            .render_frame(0.0, &OrbitControls::default(), None)
            .unwrap();
        assert_eq!(frame.width, 512);
        assert_eq!(frame.height, 512);
        assert!(frame.stats.shaded_pixels > 1_000);
    }

    #[test]
    fn segment_registry_resolves_texture_and_display_list_bindings() {
        let runtime = Fast3dRuntime::load_from_path(&fixture_manifest_path()).unwrap();
        assert_eq!(
            runtime.texture_segments.get(&1).map(String::as_str),
            Some("castle_checker")
        );
        assert_eq!(
            runtime.display_list_segments.get(&10).map(String::as_str),
            Some("quad_face")
        );
    }
}
