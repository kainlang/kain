use std::{borrow::Cow, collections::BTreeMap};

use crate::renderer::FrameDiagnostics;
use crate::{ColorRgb, Transform, Vec3};

#[derive(Clone, Debug, PartialEq)]
pub struct Camera {
    pub target: Vec3,
    pub up: Vec3,
    pub orbit_radius: f32,
    pub orbit_height: f32,
    pub orbit_speed_radians_per_second: f32,
    pub fov_y_degrees: f32,
    pub near_plane: f32,
    pub far_plane: f32,
}

#[derive(Clone, Debug, PartialEq)]
pub struct CameraPose {
    pub position: Vec3,
    pub target: Vec3,
    pub up: Vec3,
    pub fov_y_degrees: f32,
    pub near_plane: f32,
    pub far_plane: f32,
}

impl CameraPose {
    pub fn forward(&self) -> Vec3 {
        (self.target - self.position).normalize()
    }

    pub fn right(&self) -> Vec3 {
        self.forward().cross(self.up).normalize()
    }
}

impl Camera {
    pub fn position_at(&self, time_seconds: f32) -> Vec3 {
        let angle = time_seconds * self.orbit_speed_radians_per_second;
        Vec3::new(
            self.target.x + angle.cos() * self.orbit_radius,
            self.target.y + self.orbit_height,
            self.target.z + angle.sin() * self.orbit_radius,
        )
    }

    pub fn pose_at(&self, time_seconds: f32) -> CameraPose {
        CameraPose {
            position: self.position_at(time_seconds),
            target: self.target,
            up: self.up,
            fov_y_degrees: self.fov_y_degrees,
            near_plane: self.near_plane,
            far_plane: self.far_plane,
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct SceneBounds {
    pub center: Vec3,
    pub half_extents: Vec3,
}

impl SceneBounds {
    pub fn radius(&self) -> f32 {
        self.half_extents.length()
    }

    pub fn span(&self) -> Vec3 {
        self.half_extents * 2.0
    }
}

fn expand_bounds_with_point(min: &mut Vec3, max: &mut Vec3, point: Vec3) {
    min.x = min.x.min(point.x);
    min.y = min.y.min(point.y);
    min.z = min.z.min(point.z);
    max.x = max.x.max(point.x);
    max.y = max.y.max(point.y);
    max.z = max.z.max(point.z);
}

fn expand_bounds_with_sphere(min: &mut Vec3, max: &mut Vec3, center: Vec3, radius: f32) {
    let radius = radius.max(0.0);
    let local_min = center - Vec3::new(radius, radius, radius);
    let local_max = center + Vec3::new(radius, radius, radius);
    expand_bounds_with_point(min, max, local_min);
    expand_bounds_with_point(min, max, local_max);
}

fn framed_camera_distance(bounds: SceneBounds, fov_y_degrees: f32, aspect_ratio: f32) -> f32 {
    let radius = bounds.radius().max(0.001);
    let half_fov_radians = (fov_y_degrees.to_radians() * 0.5).clamp(0.2, 1.3);
    let fit_distance = radius / half_fov_radians.tan();
    let aspect_ratio = aspect_ratio.max(0.1);
    let horizontal_half_fov_radians = (half_fov_radians.tan() * aspect_ratio).atan();
    let horizontal_fit_distance = radius / horizontal_half_fov_radians.tan();
    fit_distance.max(horizontal_fit_distance).max(radius * 2.0) + radius * 0.35
}

fn framed_camera_direction(bounds: SceneBounds, aspect_ratio: f32) -> Vec3 {
    let half_extents = bounds.half_extents;
    let radius = bounds.radius().max(0.001);
    let aspect_ratio = aspect_ratio.max(0.1);
    let horizontal_bias = if aspect_ratio >= 1.0 {
        1.0 / aspect_ratio.sqrt()
    } else {
        1.0 + (1.0 - aspect_ratio).min(1.0) * 0.35
    };
    let vertical_bias = 1.0 + (half_extents.y / radius) * 0.35;

    let direction = Vec3::new(
        half_extents.x.max(0.001) * horizontal_bias,
        half_extents.y.max(0.001) * vertical_bias,
        half_extents.z.max(0.001) * horizontal_bias,
    );

    direction.normalized_or(Vec3::UP)
}

fn framed_camera_clip_planes(bounds: SceneBounds, distance: f32) -> (f32, f32) {
    let radius = bounds.radius().max(0.001);
    let near_plane = (distance - radius * 2.5).max(0.05);
    let far_plane = (distance + radius * 2.5).max(near_plane + 1.0);
    (near_plane, far_plane)
}

#[derive(Clone, Debug, PartialEq)]
pub struct SceneCompositionSummary {
    pub mesh_count: usize,
    pub material_count: usize,
    pub instance_count: usize,
    pub animation_count: usize,
    pub particle_emitter_count: usize,
    pub directional_light_count: usize,
    pub point_light_count: usize,
    pub bounds: Option<SceneBounds>,
    pub framed_camera_distance: Option<f32>,
    pub viewport_aspect_ratio: Option<f32>,
}

#[derive(Clone, Debug, PartialEq)]
pub struct SceneCompositionDiagnostics {
    pub label: String,
    pub mesh_count: usize,
    pub material_count: usize,
    pub instance_count: usize,
    pub animation_count: usize,
    pub particle_emitter_count: usize,
    pub directional_light_count: usize,
    pub point_light_count: usize,
    pub viewport_aspect_ratio: Option<f32>,
    pub framed_camera_distance: Option<f32>,
    pub bounds: Option<SceneBounds>,
    pub framing_hint: Option<&'static str>,
    pub camera_fit_ratio: Option<String>,
}

impl SceneCompositionSummary {
    fn authored_element_count(&self) -> usize {
        self.mesh_count
            + self.material_count
            + self.instance_count
            + self.animation_count
            + self.particle_emitter_count
            + self.directional_light_count
            + self.point_light_count
    }

    pub fn framing_hint_label(&self) -> Option<&'static str> {
        let bounds = self.bounds?;
        let distance = self.framed_camera_distance?;
        let radius = bounds.radius().max(0.001);
        let fit_ratio = distance / radius;

        Some(if fit_ratio < 2.8 {
            "tight-fit"
        } else if fit_ratio < 4.2 {
            "balanced-fit"
        } else {
            "loose-fit"
        })
    }

    pub fn diagnostics(&self) -> SceneCompositionDiagnostics {
        SceneCompositionDiagnostics {
            label: self.brief_label(),
            mesh_count: self.mesh_count,
            material_count: self.material_count,
            instance_count: self.instance_count,
            animation_count: self.animation_count,
            particle_emitter_count: self.particle_emitter_count,
            directional_light_count: self.directional_light_count,
            point_light_count: self.point_light_count,
            viewport_aspect_ratio: self.viewport_aspect_ratio,
            framed_camera_distance: self.framed_camera_distance,
            bounds: self.bounds,
            framing_hint: self.framing_hint_label(),
            camera_fit_ratio: self.camera_fit_ratio_label(),
        }
    }

    pub fn populate_frame_diagnostics(&self, diagnostics: &mut FrameDiagnostics) {
        diagnostics.composition_summary = Some(self.brief_label());
        diagnostics.framing_hint = self.framing_hint_label().map(str::to_string);
        diagnostics.camera_fit_ratio = self.camera_fit_ratio_label();
    }

    fn camera_fit_ratio_label(&self) -> Option<String> {
        self.bounds
            .zip(self.framed_camera_distance)
            .map(|(bounds, distance)| format!("{:.2}", distance / bounds.radius().max(0.001)))
    }

    pub fn brief_label(&self) -> String {
        let mut parts = vec![
            format!("{} meshes", self.mesh_count),
            format!("{} materials", self.material_count),
            format!("{} instances", self.instance_count),
            format!("{} elements", self.authored_element_count()),
        ];

        if self.animation_count > 0 {
            parts.push(format!("{} animations", self.animation_count));
        }
        if self.particle_emitter_count > 0 {
            parts.push(format!("{} emitters", self.particle_emitter_count));
        }
        if self.directional_light_count > 0 {
            parts.push(format!(
                "{} directional lights",
                self.directional_light_count
            ));
        }
        if self.point_light_count > 0 {
            parts.push(format!("{} point lights", self.point_light_count));
        }

        let bounds = self
            .bounds
            .map(|bounds| {
                let span = bounds.span();
                format!(
                    "bounds r{:.2} span {:.2}x{:.2}x{:.2}",
                    bounds.radius(),
                    span.x,
                    span.y,
                    span.z
                )
            })
            .unwrap_or_else(|| "unbounded".to_string());

        let camera = self
            .framed_camera_distance
            .map(|distance| format!("fit d{:.2}", distance))
            .unwrap_or_else(|| "unframed".to_string());

        let aspect = self
            .viewport_aspect_ratio
            .map(|aspect_ratio| format!("aspect {:.2}:1", aspect_ratio))
            .unwrap_or_else(|| "aspect unknown".to_string());

        format!(
            "{} | {} | {} | {}",
            parts.join(", "),
            bounds,
            camera,
            aspect
        )
    }
}

impl std::fmt::Display for SceneCompositionSummary {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(&self.brief_label())
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct Material {
    pub base_color: ColorRgb,
    pub specular_color: ColorRgb,
    pub ambient_strength: f32,
    pub diffuse_strength: f32,
    pub specular_strength: f32,
    pub shininess: f32,
}

#[derive(Clone, Debug, PartialEq)]
pub struct DirectionalLight {
    pub direction: Vec3,
    pub color: ColorRgb,
    pub intensity: f32,
}

#[derive(Clone, Debug, PartialEq)]
pub struct PointLight {
    pub position: Vec3,
    pub color: ColorRgb,
    pub intensity: f32,
    pub range: f32,
}

#[derive(Clone, Debug, PartialEq)]
pub struct LightingRig {
    pub ambient_color: ColorRgb,
    pub ambient_intensity: f32,
    pub directional_lights: Vec<DirectionalLight>,
    pub point_lights: Vec<PointLight>,
}

#[derive(Clone, Debug, PartialEq)]
pub struct BackgroundGradient {
    pub top: ColorRgb,
    pub bottom: ColorRgb,
}

#[derive(Clone, Debug, PartialEq)]
pub struct Vertex {
    pub position: Vec3,
    pub normal: Vec3,
}

#[derive(Clone, Debug, PartialEq)]
pub struct Mesh {
    pub vertices: Vec<Vertex>,
    pub triangles: Vec<[usize; 3]>,
}

#[derive(Clone, Debug, PartialEq)]
pub struct SceneInstance {
    pub id: String,
    pub mesh: String,
    pub material: String,
    pub transform: Transform,
}

#[derive(Clone, Debug, PartialEq)]
pub enum SceneAnimation {
    Spin {
        instance_id: String,
        axis_radians_per_second: Vec3,
    },
    Bob {
        instance_id: String,
        amplitude: f32,
        speed_radians_per_second: f32,
    },
}

#[derive(Clone, Debug, PartialEq)]
pub struct ParticleEmitter {
    pub id: String,
    pub center: Vec3,
    pub axis: Vec3,
    pub radial_range: [f32; 2],
    pub vertical_range: [f32; 2],
    pub particle_size_range: [f32; 2],
    pub particle_count: usize,
    pub orbit_radians_per_second: f32,
    pub swirl: f32,
    pub drift: Vec3,
    pub color_start: ColorRgb,
    pub color_end: ColorRgb,
    pub emissive_strength: f32,
    pub softness: f32,
    pub depth_test: bool,
}

impl ParticleEmitter {
    pub fn axis_or_up(&self) -> Vec3 {
        self.axis.normalized_or(Vec3::UP)
    }

    pub fn bounds(&self) -> SceneBounds {
        let radial_radius = self.radial_range[0].abs().max(self.radial_range[1].abs());
        let vertical_radius = self.vertical_range[0]
            .abs()
            .max(self.vertical_range[1].abs());
        let particle_radius = self.particle_size_range[0]
            .abs()
            .max(self.particle_size_range[1].abs());
        let radius = radial_radius.max(vertical_radius).max(particle_radius) + self.drift.length();
        SceneBounds {
            center: self.center,
            half_extents: Vec3::new(radius, radius, radius),
        }
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct SceneDescription {
    pub name: String,
    pub viewport_summary: String,
    pub background: BackgroundGradient,
    pub camera: Camera,
    pub lighting: LightingRig,
    pub meshes: BTreeMap<String, Mesh>,
    pub materials: BTreeMap<String, Material>,
    pub instances: Vec<SceneInstance>,
    pub animations: Vec<SceneAnimation>,
    pub particle_emitters: Vec<ParticleEmitter>,
}

impl SceneDescription {
    pub fn animated_instances(&self, time_seconds: f32) -> Vec<SceneInstance> {
        let mut instances = self.instances.clone();
        for animation in &self.animations {
            match animation {
                SceneAnimation::Spin {
                    instance_id,
                    axis_radians_per_second,
                } => {
                    if let Some(instance) = instances
                        .iter_mut()
                        .find(|candidate| candidate.id == *instance_id)
                    {
                        instance.transform.rotation_radians +=
                            *axis_radians_per_second * time_seconds;
                    }
                }
                SceneAnimation::Bob {
                    instance_id,
                    amplitude,
                    speed_radians_per_second,
                } => {
                    if let Some(instance) = instances
                        .iter_mut()
                        .find(|candidate| candidate.id == *instance_id)
                    {
                        instance.transform.translation.y +=
                            amplitude * (time_seconds * speed_radians_per_second).sin();
                    }
                }
            }
        }
        instances
    }

    pub fn animated_instances_with_overrides(
        &self,
        time_seconds: f32,
        instance_transform_overrides: &BTreeMap<String, Transform>,
    ) -> Vec<SceneInstance> {
        let mut instances = self.animated_instances(time_seconds);
        if instance_transform_overrides.is_empty() {
            return instances;
        }
        for instance in &mut instances {
            if let Some(override_transform) = instance_transform_overrides.get(&instance.id) {
                instance.transform = override_transform.clone();
            }
        }
        instances
    }

    pub fn resolved_mesh(&self, mesh_id: &str, _time_seconds: f32) -> Option<Cow<'_, Mesh>> {
        self.meshes.get(mesh_id).map(Cow::Borrowed)
    }

    pub fn ground_height_at(&self, _world_position: Vec3, _time_seconds: f32) -> Option<f32> {
        None
    }

    pub fn bounds(&self, time_seconds: f32) -> Option<SceneBounds> {
        self.bounds_with_overrides(time_seconds, &BTreeMap::new())
    }

    pub fn bounds_with_overrides(
        &self,
        time_seconds: f32,
        instance_transform_overrides: &BTreeMap<String, Transform>,
    ) -> Option<SceneBounds> {
        let mut min = Vec3::new(f32::INFINITY, f32::INFINITY, f32::INFINITY);
        let mut max = Vec3::new(f32::NEG_INFINITY, f32::NEG_INFINITY, f32::NEG_INFINITY);
        let mut found = false;

        for instance in
            self.animated_instances_with_overrides(time_seconds, instance_transform_overrides)
        {
            if let Some(mesh) = self.resolved_mesh(&instance.mesh, time_seconds) {
                for vertex in &mesh.vertices {
                    expand_bounds_with_point(
                        &mut min,
                        &mut max,
                        instance.transform.transform_point(vertex.position),
                    );
                    found = true;
                }
            }
        }

        for emitter in &self.particle_emitters {
            let emitter_bounds = emitter.bounds();
            expand_bounds_with_sphere(
                &mut min,
                &mut max,
                emitter_bounds.center,
                emitter_bounds.radius(),
            );
            found = true;
        }

        if !found {
            return None;
        }

        Some(SceneBounds {
            center: (min + max) * 0.5,
            half_extents: (max - min) * 0.5,
        })
    }

    pub fn framed_camera_pose(&self, time_seconds: f32, aspect_ratio: f32) -> CameraPose {
        self.framed_camera_pose_with_overrides(time_seconds, aspect_ratio, &BTreeMap::new())
    }

    pub fn framed_camera_pose_with_overrides(
        &self,
        time_seconds: f32,
        aspect_ratio: f32,
        instance_transform_overrides: &BTreeMap<String, Transform>,
    ) -> CameraPose {
        let bounds = self.bounds_with_overrides(time_seconds, instance_transform_overrides);
        if let Some(bounds) = bounds {
            let distance = framed_camera_distance(bounds, self.camera.fov_y_degrees, aspect_ratio);
            let (near_plane, far_plane) = framed_camera_clip_planes(bounds, distance);
            let framing_direction = framed_camera_direction(bounds, aspect_ratio);
            CameraPose {
                position: bounds.center + framing_direction * distance,
                target: bounds.center,
                up: Vec3::UP,
                fov_y_degrees: self.camera.fov_y_degrees,
                near_plane,
                far_plane,
            }
        } else {
            self.camera.pose_at(time_seconds)
        }
    }

    pub fn composition_summary(&self, time_seconds: f32) -> SceneCompositionSummary {
        self.composition_summary_with_aspect_ratio(time_seconds, 1.0)
    }

    pub fn composition_summary_with_aspect_ratio(
        &self,
        time_seconds: f32,
        aspect_ratio: f32,
    ) -> SceneCompositionSummary {
        let bounds = self.bounds(time_seconds);
        SceneCompositionSummary {
            mesh_count: self.meshes.len(),
            material_count: self.materials.len(),
            instance_count: self.instances.len(),
            animation_count: self.animations.len(),
            particle_emitter_count: self.particle_emitters.len(),
            directional_light_count: self.lighting.directional_lights.len(),
            point_light_count: self.lighting.point_lights.len(),
            framed_camera_distance: bounds.map(|bounds| {
                framed_camera_distance(bounds, self.camera.fov_y_degrees, aspect_ratio)
            }),
            viewport_aspect_ratio: Some(aspect_ratio),
            bounds,
        }
    }

    pub fn composition_summary_with_overrides(
        &self,
        time_seconds: f32,
        instance_transform_overrides: &BTreeMap<String, Transform>,
    ) -> SceneCompositionSummary {
        self.composition_summary_with_overrides_and_aspect_ratio(
            time_seconds,
            instance_transform_overrides,
            1.0,
        )
    }

    pub fn composition_summary_with_overrides_and_aspect_ratio(
        &self,
        time_seconds: f32,
        instance_transform_overrides: &BTreeMap<String, Transform>,
        aspect_ratio: f32,
    ) -> SceneCompositionSummary {
        let bounds = self.bounds_with_overrides(time_seconds, instance_transform_overrides);
        SceneCompositionSummary {
            mesh_count: self.meshes.len(),
            material_count: self.materials.len(),
            instance_count: self.instances.len(),
            animation_count: self.animations.len(),
            particle_emitter_count: self.particle_emitters.len(),
            directional_light_count: self.lighting.directional_lights.len(),
            point_light_count: self.lighting.point_lights.len(),
            framed_camera_distance: bounds.map(|bounds| {
                framed_camera_distance(bounds, self.camera.fov_y_degrees, aspect_ratio)
            }),
            viewport_aspect_ratio: Some(aspect_ratio),
            bounds,
        }
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct SceneCatalog {
    pub default_scene: String,
    pub scenes: BTreeMap<String, SceneDescription>,
    pub scene_aliases: BTreeMap<String, String>,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum SceneCatalogError {
    EmptyDefaultScene,
    MissingDefaultScene(String),
    EmptySceneName,
    SceneNameMismatch { key: String, scene_name: String },
    AliasConflictsWithScene(String),
    AliasTargetsMissing { alias: String, target: String },
}

impl std::fmt::Display for SceneCatalogError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::EmptyDefaultScene => f.write_str("scene catalog default scene is empty"),
            Self::MissingDefaultScene(name) => {
                write!(f, "scene catalog default scene `{name}` is not registered")
            }
            Self::EmptySceneName => f.write_str("scene catalog contains an empty scene name"),
            Self::SceneNameMismatch { key, scene_name } => write!(
                f,
                "scene catalog key `{key}` does not match scene name `{scene_name}`"
            ),
            Self::AliasConflictsWithScene(alias) => {
                write!(f, "scene alias `{alias}` conflicts with a canonical scene")
            }
            Self::AliasTargetsMissing { alias, target } => {
                write!(f, "scene alias `{alias}` targets missing scene `{target}`")
            }
        }
    }
}

impl std::error::Error for SceneCatalogError {}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct SceneCatalogSummary {
    pub default_scene: String,
    pub canonical_scene_count: usize,
    pub alias_count: usize,
    pub total_scene_names: usize,
}

#[derive(Clone, Debug, PartialEq)]
pub struct SceneCatalogEntry<'a> {
    pub requested_name: &'a str,
    pub resolved_name: &'a str,
    pub is_alias: bool,
    pub is_default: bool,
    pub viewport_summary: &'a str,
}

impl<'a> SceneCatalogEntry<'a> {
    pub fn picker_label(&self) -> String {
        let mut parts = vec![
            self.resolved_name.to_string(),
            self.viewport_summary.to_string(),
        ];
        if self.is_default {
            parts.push("default".to_string());
        }
        if self.is_alias {
            parts.push(format!("alias:{}", self.requested_name));
        }
        parts.join(" | ")
    }
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum SceneResolutionKind {
    Exact,
    Alias { alias: String },
    Default { requested: String },
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct SceneResolution {
    pub requested_name: String,
    pub resolved_name: String,
    pub kind: SceneResolutionKind,
}

pub struct ResolvedScene<'a> {
    pub scene: &'a SceneDescription,
    pub resolution: SceneResolution,
}

impl SceneCatalog {
    pub fn new(
        default_scene: impl Into<String>,
        scenes: BTreeMap<String, SceneDescription>,
        scene_aliases: BTreeMap<String, String>,
    ) -> Result<Self, SceneCatalogError> {
        let default_scene = default_scene.into();
        if default_scene.is_empty() {
            return Err(SceneCatalogError::EmptyDefaultScene);
        }
        if !scenes.contains_key(&default_scene) {
            return Err(SceneCatalogError::MissingDefaultScene(default_scene));
        }
        for (key, scene) in &scenes {
            if key.is_empty() || scene.name.is_empty() {
                return Err(SceneCatalogError::EmptySceneName);
            }
            if key != &scene.name {
                return Err(SceneCatalogError::SceneNameMismatch {
                    key: key.clone(),
                    scene_name: scene.name.clone(),
                });
            }
        }
        for (alias, target) in &scene_aliases {
            if scenes.contains_key(alias) {
                return Err(SceneCatalogError::AliasConflictsWithScene(alias.clone()));
            }
            if !scenes.contains_key(target) {
                return Err(SceneCatalogError::AliasTargetsMissing {
                    alias: alias.clone(),
                    target: target.clone(),
                });
            }
        }

        Ok(Self {
            default_scene,
            scenes,
            scene_aliases,
        })
    }

    pub fn single(scene: SceneDescription) -> Self {
        let default_scene = scene.name.clone();
        let scenes = BTreeMap::from([(scene.name.clone(), scene)]);
        Self {
            default_scene,
            scenes,
            scene_aliases: BTreeMap::new(),
        }
    }

    pub fn empty() -> Self {
        Self {
            default_scene: String::new(),
            scenes: BTreeMap::new(),
            scene_aliases: BTreeMap::new(),
        }
    }

    pub fn summary(&self) -> SceneCatalogSummary {
        SceneCatalogSummary {
            default_scene: self.default_scene.clone(),
            canonical_scene_count: self.scenes.len(),
            alias_count: self.scene_aliases.len(),
            total_scene_names: self.scenes.len() + self.scene_aliases.len(),
        }
    }

    pub fn scene(&self, name: &str) -> Option<&SceneDescription> {
        self.resolve_scene(name).map(|resolution| resolution.scene)
    }

    pub fn scene_names(&self) -> impl Iterator<Item = &str> {
        self.scenes.keys().map(String::as_str)
    }

    pub fn scene_names_with_aliases(&self) -> impl Iterator<Item = &str> {
        self.scenes
            .keys()
            .chain(self.scene_aliases.keys())
            .map(String::as_str)
    }

    pub fn catalog_entries(&self) -> impl Iterator<Item = SceneCatalogEntry<'_>> {
        self.scene_names().map(move |name| self.catalog_entry(name))
    }

    pub fn catalog_entries_with_aliases(&self) -> impl Iterator<Item = SceneCatalogEntry<'_>> {
        self.scene_names_with_aliases()
            .map(move |name| self.catalog_entry(name))
    }

    pub fn picker_entries(&self) -> Vec<SceneCatalogEntry<'_>> {
        let mut entries = Vec::with_capacity(self.scenes.len() + self.scene_aliases.len());

        if self.scenes.contains_key(&self.default_scene) {
            entries.push(self.catalog_entry(&self.default_scene));
        }

        entries.extend(
            self.scene_names()
                .filter(|name| *name != self.default_scene)
                .map(|name| self.catalog_entry(name)),
        );
        entries.extend(
            self.scene_aliases
                .keys()
                .map(|name| self.catalog_entry(name)),
        );

        entries
    }

    pub fn catalog_entry<'a>(&'a self, requested_name: &'a str) -> SceneCatalogEntry<'a> {
        let resolved = self
            .resolve_scene(requested_name)
            .expect("catalog entries should resolve through the scene catalog");
        SceneCatalogEntry {
            requested_name,
            resolved_name: &resolved.scene.name,
            is_alias: matches!(resolved.resolution.kind, SceneResolutionKind::Alias { .. }),
            is_default: requested_name == self.default_scene
                || matches!(
                    resolved.resolution.kind,
                    SceneResolutionKind::Default { .. }
                ),
            viewport_summary: &resolved.scene.viewport_summary,
        }
    }

    pub fn resolve_scene(&self, name: &str) -> Option<ResolvedScene<'_>> {
        if let Some(scene) = self.scenes.get(name) {
            return Some(ResolvedScene {
                scene,
                resolution: SceneResolution {
                    requested_name: name.to_string(),
                    resolved_name: scene.name.clone(),
                    kind: SceneResolutionKind::Exact,
                },
            });
        }

        if let Some(canonical) = self.scene_aliases.get(name) {
            if let Some(scene) = self.scenes.get(canonical) {
                return Some(ResolvedScene {
                    scene,
                    resolution: SceneResolution {
                        requested_name: name.to_string(),
                        resolved_name: scene.name.clone(),
                        kind: SceneResolutionKind::Alias {
                            alias: name.to_string(),
                        },
                    },
                });
            }
        }

        if self.default_scene.is_empty() {
            return None;
        }

        self.scenes
            .get(&self.default_scene)
            .map(|scene| ResolvedScene {
                scene,
                resolution: SceneResolution {
                    requested_name: name.to_string(),
                    resolved_name: scene.name.clone(),
                    kind: SceneResolutionKind::Default {
                        requested: name.to_string(),
                    },
                },
            })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn test_material() -> Material {
        Material {
            base_color: ColorRgb::new(0.2, 0.6, 0.9),
            specular_color: ColorRgb::WHITE,
            ambient_strength: 0.2,
            diffuse_strength: 0.8,
            specular_strength: 0.2,
            shininess: 12.0,
        }
    }

    fn test_mesh() -> Mesh {
        Mesh {
            vertices: vec![
                Vertex {
                    position: Vec3::new(-1.0, -1.0, 0.0),
                    normal: Vec3::new(0.0, 0.0, 1.0),
                },
                Vertex {
                    position: Vec3::new(1.0, -1.0, 0.0),
                    normal: Vec3::new(0.0, 0.0, 1.0),
                },
                Vertex {
                    position: Vec3::new(0.0, 1.0, 0.0),
                    normal: Vec3::new(0.0, 0.0, 1.0),
                },
            ],
            triangles: vec![[0, 1, 2]],
        }
    }

    fn test_scene(name: &str) -> SceneDescription {
        SceneDescription {
            name: name.to_string(),
            viewport_summary: "explicit test scene".to_string(),
            background: BackgroundGradient {
                top: ColorRgb::BLACK,
                bottom: ColorRgb::BLACK,
            },
            camera: Camera {
                target: Vec3::ZERO,
                up: Vec3::UP,
                orbit_radius: 6.0,
                orbit_height: 1.5,
                orbit_speed_radians_per_second: 0.0,
                fov_y_degrees: 60.0,
                near_plane: 0.1,
                far_plane: 100.0,
            },
            lighting: LightingRig {
                ambient_color: ColorRgb::WHITE,
                ambient_intensity: 0.2,
                directional_lights: vec![DirectionalLight {
                    direction: Vec3::new(0.0, -1.0, -1.0).normalize(),
                    color: ColorRgb::WHITE,
                    intensity: 1.0,
                }],
                point_lights: vec![],
            },
            meshes: BTreeMap::from([("triangle".to_string(), test_mesh())]),
            materials: BTreeMap::from([("matte".to_string(), test_material())]),
            instances: vec![SceneInstance {
                id: "hero".to_string(),
                mesh: "triangle".to_string(),
                material: "matte".to_string(),
                transform: Transform::identity().with_translation(Vec3::new(4.0, 0.0, 0.0)),
            }],
            animations: vec![SceneAnimation::Bob {
                instance_id: "hero".to_string(),
                amplitude: 1.0,
                speed_radians_per_second: std::f32::consts::FRAC_PI_2,
            }],
            particle_emitters: vec![],
        }
    }

    #[test]
    fn explicit_catalog_resolves_exact_alias_and_default() {
        let scene = test_scene("authored_scene");
        let catalog = SceneCatalog::new(
            "authored_scene",
            BTreeMap::from([("authored_scene".to_string(), scene)]),
            BTreeMap::from([("preview".to_string(), "authored_scene".to_string())]),
        )
        .expect("catalog should validate");

        let exact = catalog
            .resolve_scene("authored_scene")
            .expect("exact scene should resolve");
        assert!(matches!(exact.resolution.kind, SceneResolutionKind::Exact));

        let alias = catalog
            .resolve_scene("preview")
            .expect("alias scene should resolve");
        assert_eq!(alias.resolution.resolved_name, "authored_scene");
        assert!(matches!(
            alias.resolution.kind,
            SceneResolutionKind::Alias { .. }
        ));

        let fallback = catalog
            .resolve_scene("missing")
            .expect("missing scene should fall back to the declared default");
        assert_eq!(fallback.resolution.resolved_name, "authored_scene");
        assert!(matches!(
            fallback.resolution.kind,
            SceneResolutionKind::Default { .. }
        ));
    }

    #[test]
    fn catalog_validation_rejects_implicit_or_dangling_scene_data() {
        let scene = test_scene("authored_scene");
        let missing_default = SceneCatalog::new(
            "missing",
            BTreeMap::from([("authored_scene".to_string(), scene.clone())]),
            BTreeMap::new(),
        )
        .expect_err("missing default should fail");
        assert!(matches!(
            missing_default,
            SceneCatalogError::MissingDefaultScene(_)
        ));

        let missing_alias_target = SceneCatalog::new(
            "authored_scene",
            BTreeMap::from([("authored_scene".to_string(), scene)]),
            BTreeMap::from([("alias".to_string(), "missing".to_string())]),
        )
        .expect_err("dangling alias should fail");
        assert!(matches!(
            missing_alias_target,
            SceneCatalogError::AliasTargetsMissing { .. }
        ));
    }

    #[test]
    fn scene_bounds_and_framed_camera_follow_authored_geometry() {
        let scene = test_scene("authored_scene");
        let bounds = scene.bounds(0.0).expect("scene should have bounds");

        assert!(bounds.center.x > 3.0);
        let framed = scene.framed_camera_pose(0.0, 1.0);
        assert_eq!(framed.target, bounds.center);
        assert!(framed.position.distance(bounds.center) > bounds.radius());
    }

    #[test]
    fn instance_overrides_are_reflected_in_bounds_and_camera() {
        let scene = test_scene("authored_scene");
        let overrides = BTreeMap::from([(
            "hero".to_string(),
            Transform::identity().with_translation(Vec3::new(20.0, 5.0, -4.0)),
        )]);

        let bounds = scene
            .bounds_with_overrides(0.0, &overrides)
            .expect("override scene should still have bounds");
        let framed = scene.framed_camera_pose_with_overrides(0.0, 1.0, &overrides);

        assert!(bounds.center.x > 19.0);
        assert!(bounds.center.y > 4.0);
        assert_eq!(framed.target, bounds.center);
    }

    #[test]
    fn particle_emitters_are_generic_bounds_contributors() {
        let mut scene = test_scene("particle_scene");
        scene.instances.clear();
        scene.particle_emitters.push(ParticleEmitter {
            id: "sparkfield".to_string(),
            center: Vec3::new(10.0, 2.0, -3.0),
            axis: Vec3::ZERO,
            radial_range: [2.0, 4.0],
            vertical_range: [1.0, 1.5],
            particle_size_range: [0.2, 0.6],
            particle_count: 64,
            orbit_radians_per_second: 0.0,
            swirl: 0.0,
            drift: Vec3::new(0.5, 0.0, 0.0),
            color_start: ColorRgb::new(1.0, 0.3, 0.2),
            color_end: ColorRgb::new(1.0, 0.9, 0.4),
            emissive_strength: 1.0,
            softness: 1.0,
            depth_test: true,
        });

        let bounds = scene
            .bounds(0.0)
            .expect("emitter-only scene should have bounds");
        assert!(bounds.center.x > 8.0);
        assert_eq!(scene.particle_emitters[0].axis_or_up(), Vec3::UP);
    }

    #[test]
    fn composition_summary_reports_structural_counts() {
        let scene = test_scene("authored_scene");
        let summary = scene.composition_summary_with_aspect_ratio(0.0, 1.5);
        let diagnostics = summary.diagnostics();

        assert_eq!(summary.mesh_count, 1);
        assert_eq!(summary.material_count, 1);
        assert_eq!(summary.instance_count, 1);
        assert_eq!(summary.animation_count, 1);
        assert_eq!(summary.directional_light_count, 1);
        assert_eq!(diagnostics.viewport_aspect_ratio, Some(1.5));
        assert!(diagnostics.label.contains("1 meshes"));
        assert!(diagnostics.framing_hint.is_some());
        assert!(diagnostics.camera_fit_ratio.is_some());
    }
}
