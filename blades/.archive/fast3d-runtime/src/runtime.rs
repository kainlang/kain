use std::{collections::HashMap, fs, path::Path};

use glam::Vec4Swizzles;

use crate::combiner::CombinerState;
use crate::host_documents::{
    ActorAnimationDefinition, Fast3dGameplayStateDocument, Fast3dShaderOverrideDocument,
};
use crate::math::{
    camera_forward, matrix_from_rows, orbit_camera_position, vec2_from_array, vec3_from_array,
    vec4_from_rgba8, Float4, Matrix4,
};
use crate::model::{
    CombineMode, DisplayListCommand, DisplayListDefinition, Fast3dSmokeManifest, Fast3dVertex,
    LightGroupDefinition, SegmentBindingKind,
};
use crate::rasterizer::{Framebuffer, RenderFrame, RenderStats, ScreenVertex};
use crate::texture::{build_texture_catalog, TextureImage};

/// First-person or free-fly camera pose for direct injection via `render_frame_with_pose`.
/// Decouples the camera from the manifest's orbit model entirely.
#[derive(Clone, Copy, Debug)]
pub struct FreeCameraPose {
    pub position: [f32; 3],
    pub yaw_radians: f32,
    pub pitch_radians: f32,
    pub fov_y_degrees: f32,
    pub near_plane: f32,
    pub far_plane: f32,
}

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
    light_groups_by_id: HashMap<String, LightGroupState>,
    texture_segments: HashMap<u8, String>,
    display_list_segments: HashMap<u8, String>,
    /// Per-actor transform overrides, keyed by display list ID.
    /// When set, this matrix is pushed onto the model stack before entering that display list,
    /// binding live gameplay transform data to the rendered actor.
    actor_override_transforms: HashMap<String, Matrix4>,
    display_list_material_overrides: HashMap<String, DisplayListMaterialOverride>,
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
        let light_groups_by_id = manifest
            .light_groups
            .iter()
            .map(|light_group| {
                (
                    light_group.id.clone(),
                    LightGroupState::from_definition(light_group),
                )
            })
            .collect::<HashMap<_, _>>();

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
            light_groups_by_id,
            texture_segments,
            display_list_segments,
            actor_override_transforms: HashMap::new(),
            display_list_material_overrides: HashMap::new(),
        })
    }

    /// Bind a gameplay transform to a named display list (actor).
    /// On every call to `render_frame` or `render_frame_with_pose`, this matrix is pushed
    /// onto the model stack before entering that display list, overriding its manifest-embedded
    /// position. This is how gameplay physics state maps to a rendered actor's position.
    pub fn set_actor_transform(&mut self, display_list_id: &str, transform: [[f32; 4]; 4]) {
        self.actor_override_transforms
            .insert(display_list_id.to_string(), matrix_from_rows(transform));
    }

    /// Remove an actor transform override, reverting to the manifest-embedded matrix.
    pub fn clear_actor_transform(&mut self, display_list_id: &str) {
        self.actor_override_transforms.remove(display_list_id);
    }

    pub fn clear_actor_transforms(&mut self) {
        self.actor_override_transforms.clear();
    }

    pub fn apply_gameplay_state(
        &mut self,
        time_seconds: f32,
        gameplay: &Fast3dGameplayStateDocument,
    ) {
        self.clear_actor_transforms();
        for binding in &gameplay.actor_bindings {
            let transform = match binding.animation {
                ActorAnimationDefinition::None => Matrix4::IDENTITY,
                ActorAnimationDefinition::SpinY { degrees_per_second } => {
                    Matrix4::from_rotation_y((degrees_per_second * time_seconds).to_radians())
                }
                ActorAnimationDefinition::BobY {
                    amplitude,
                    cycles_per_second,
                    base_height,
                } => {
                    let offset = base_height
                        + amplitude
                            * (time_seconds * cycles_per_second * std::f32::consts::TAU).sin();
                    Matrix4::from_translation(glam::Vec3::new(0.0, offset, 0.0))
                }
            };
            self.actor_override_transforms
                .insert(binding.display_list_id.clone(), transform);
        }
    }

    pub fn apply_shader_overrides(&mut self, overrides: &Fast3dShaderOverrideDocument) {
        self.display_list_material_overrides.clear();
        for override_entry in &overrides.display_list_overrides {
            self.display_list_material_overrides.insert(
                override_entry.display_list_id.clone(),
                DisplayListMaterialOverride {
                    combine_mode: override_entry.combine_mode,
                    primitive_color: override_entry.primitive_color.map(vec4_from_rgba8),
                    env_color: override_entry.env_color.map(vec4_from_rgba8),
                },
            );
        }
    }

    /// Render a frame using an explicit free-fly/first-person camera pose instead of
    /// the manifest's orbit camera. Used by `FreeFlyControls` in the viewer.
    pub fn render_frame_with_pose(
        &self,
        pose: &FreeCameraPose,
        combine_override: Option<CombineMode>,
    ) -> Result<RenderFrame, String> {
        let resolution = self.manifest.resolution;
        let mut framebuffer = Framebuffer::new(
            resolution.width,
            resolution.height,
            self.manifest.clear_color,
        );
        let mut stats = RenderStats::default();
        let view_projection = build_free_camera_view_projection(
            pose,
            resolution.width as f32 / resolution.height as f32,
        );
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

    pub fn render_frame(
        &self,
        time_seconds: f32,
        orbit_controls: &OrbitControls,
        combine_override: Option<CombineMode>,
    ) -> Result<RenderFrame, String> {
        let resolution = self.manifest.resolution;
        let mut framebuffer = Framebuffer::new(
            resolution.width,
            resolution.height,
            self.manifest.clear_color,
        );
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

    fn build_view_projection(&self, time_seconds: f32, orbit_controls: &OrbitControls) -> Matrix4 {
        let camera = self.manifest.camera;
        let yaw = camera.initial_yaw_radians
            + time_seconds * self.manifest.auto_rotation_radians_per_second
            + orbit_controls.yaw_radians;
        let radius = (camera.orbit_radius + orbit_controls.zoom_delta).max(1.25);
        let target = vec3_from_array(camera.target);
        let position = orbit_camera_position(
            target,
            radius,
            camera.orbit_height,
            yaw,
            camera.initial_pitch_radians + orbit_controls.pitch_radians,
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
        let saved_material_state = execution.save_material_state();
        if let Some(material_override) = self.display_list_material_overrides.get(display_list_id) {
            material_override.apply(execution);
        }

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
                            self.project_vertex(
                                left,
                                execution.current_light_group,
                                current_model,
                                view_projection,
                                framebuffer,
                            )?,
                            self.project_vertex(
                                middle,
                                execution.current_light_group,
                                current_model,
                                view_projection,
                                framebuffer,
                            )?,
                            self.project_vertex(
                                right,
                                execution.current_light_group,
                                current_model,
                                view_projection,
                                framebuffer,
                            )?,
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
                    // Check for a gameplay-driven actor transform override.
                    // If present, push the override matrix before entering the sub-list so
                    // every vertex in that display list renders in the overridden world position.
                    if let Some(&override_matrix) =
                        self.actor_override_transforms.get(display_list_id)
                    {
                        let parent = *execution
                            .matrix_stack
                            .last()
                            .ok_or("matrix stack unexpectedly empty")?;
                        execution.matrix_stack.push(parent * override_matrix);
                        self.execute_display_list(
                            display_list_id,
                            view_projection,
                            combine_override,
                            execution,
                            framebuffer,
                            stats,
                        )?;
                        execution.matrix_stack.pop();
                    } else {
                        self.execute_display_list(
                            display_list_id,
                            view_projection,
                            combine_override,
                            execution,
                            framebuffer,
                            stats,
                        )?;
                    }
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
                DisplayListCommand::SetLightGroup { light_group_id } => {
                    let light_group = self
                        .light_groups_by_id
                        .get(light_group_id)
                        .copied()
                        .ok_or_else(|| format!("missing light group `{light_group_id}`"))?;
                    execution.current_light_group = Some(light_group);
                }
            }
        }
        execution.restore_material_state(saved_material_state);
        Ok(())
    }

    fn project_vertex(
        &self,
        vertex: LoadedVertex,
        current_light_group: Option<LightGroupState>,
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
        let color = vertex
            .normal
            .zip(current_light_group)
            .map(|(normal, light_group)| {
                let transformed_normal = (model_matrix * normal.extend(0.0))
                    .xyz()
                    .normalize_or_zero();
                light_group.shade_world_normal(transformed_normal, vertex.base_color.w)
            })
            .unwrap_or(vertex.base_color);
        Ok(ScreenVertex {
            x,
            y,
            depth: ndc.z,
            inv_w,
            uv_over_w: vertex.uv * inv_w,
            color_over_w: color * inv_w,
        })
    }
}

#[derive(Clone, Copy, Debug)]
struct LoadedVertex {
    position: glam::Vec3,
    uv: glam::Vec2,
    base_color: Float4,
    normal: Option<glam::Vec3>,
}

#[derive(Clone, Debug)]
struct ExecutionState {
    matrix_stack: Vec<Matrix4>,
    vertex_cache: Vec<Option<LoadedVertex>>,
    bound_texture: Option<String>,
    combine_mode: CombineMode,
    primitive_color: Float4,
    env_color: Float4,
    current_light_group: Option<LightGroupState>,
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
            current_light_group: None,
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
                base_color: vec4_from_rgba8(vertex.color),
                normal: vertex.normal.map(vec3_from_array),
            });
        }
        Ok(())
    }

    fn save_material_state(&self) -> SavedMaterialState {
        SavedMaterialState {
            combine_mode: self.combine_mode,
            primitive_color: self.primitive_color,
            env_color: self.env_color,
        }
    }

    fn restore_material_state(&mut self, saved: SavedMaterialState) {
        self.combine_mode = saved.combine_mode;
        self.primitive_color = saved.primitive_color;
        self.env_color = saved.env_color;
    }
}

#[derive(Clone, Copy, Debug)]
struct SavedMaterialState {
    combine_mode: CombineMode,
    primitive_color: Float4,
    env_color: Float4,
}

#[derive(Clone, Copy, Debug, Default)]
struct DisplayListMaterialOverride {
    combine_mode: Option<CombineMode>,
    primitive_color: Option<Float4>,
    env_color: Option<Float4>,
}

impl DisplayListMaterialOverride {
    fn apply(&self, execution: &mut ExecutionState) {
        if let Some(combine_mode) = self.combine_mode {
            execution.combine_mode = combine_mode;
        }
        if let Some(primitive_color) = self.primitive_color {
            execution.primitive_color = primitive_color;
        }
        if let Some(env_color) = self.env_color {
            execution.env_color = env_color;
        }
    }
}

#[derive(Clone, Copy, Debug)]
struct LightGroupState {
    ambient_color: Float4,
    diffuse_color: Float4,
    direction: glam::Vec3,
}

impl LightGroupState {
    fn from_definition(definition: &LightGroupDefinition) -> Self {
        Self {
            ambient_color: vec4_from_rgba8(definition.ambient_color),
            diffuse_color: vec4_from_rgba8(definition.diffuse_color),
            direction: vec3_from_array(definition.direction).normalize_or_zero(),
        }
    }

    fn shade_world_normal(&self, normal: glam::Vec3, alpha: f32) -> Float4 {
        let normal = normal.normalize_or_zero();
        let diffuse_strength = normal.dot(self.direction).max(0.0);
        let shaded_rgb =
            self.ambient_color.truncate() + self.diffuse_color.truncate() * diffuse_strength;
        glam::Vec4::new(
            shaded_rgb.x.clamp(0.0, 1.0),
            shaded_rgb.y.clamp(0.0, 1.0),
            shaded_rgb.z.clamp(0.0, 1.0),
            alpha,
        )
    }
}

/// Build a view-projection matrix from a free-fly camera pose.
/// Uses yaw/pitch to compute a look-at forward vector; does not orbit around a target.
fn build_free_camera_view_projection(pose: &FreeCameraPose, aspect_ratio: f32) -> Matrix4 {
    let position = vec3_from_array(pose.position);
    let forward = camera_forward(pose.yaw_radians, pose.pitch_radians);
    let view = Matrix4::look_at_rh(position, position + forward, glam::Vec3::Y);
    let projection = Matrix4::perspective_rh(
        pose.fov_y_degrees.to_radians(),
        aspect_ratio,
        pose.near_plane,
        pose.far_plane,
    );
    projection * view
}

#[cfg(test)]
mod tests {
    use std::path::PathBuf;

    use super::*;

    fn fixture_manifest_path() -> PathBuf {
        PathBuf::from(env!("CARGO_MANIFEST_DIR"))
            .join("..")
            .join("..")
            .join("smoketest")
            .join("3D")
            .join("sm64_fast3d_smoke")
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
