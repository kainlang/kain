use std::{borrow::Cow, collections::BTreeMap};

use crate::primitive::PrimitiveShape;
use crate::{
    ColorRgb, Effector, Field, InstancePattern, Instancer, NodeId, PrimitiveLibrary,
    Scene as AuthoringScene, Transform, Vec3,
};

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
        let axis = self.axis.normalize();
        if axis.length() <= f32::EPSILON {
            Vec3::UP
        } else {
            axis
        }
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct BlackHole {
    pub center: Vec3,
    pub radius: f32,
    pub lens_radius: f32,
    pub spin_axis: Vec3,
    pub inner_color: ColorRgb,
    pub lens_color: ColorRgb,
    pub disk_color: ColorRgb,
}

#[derive(Clone, Debug, PartialEq)]
pub struct TerrainSurface {
    pub id: String,
    pub mesh_id: String,
    pub center: Vec3,
    pub half_extents: Vec3,
    pub resolution: [usize; 2],
    pub base_height: f32,
    pub height_amplitude: f32,
    pub terrace_step: f32,
    pub rim_strength: f32,
    pub ripple_amplitude: f32,
    pub ripple_frequency: f32,
    pub flow_speed: f32,
    pub caldera_radius: f32,
    pub caldera_depth: f32,
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
    pub black_hole: Option<BlackHole>,
    pub terrain_surfaces: Vec<TerrainSurface>,
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

    pub fn resolved_mesh(&self, mesh_id: &str, time_seconds: f32) -> Option<Cow<'_, Mesh>> {
        if let Some(mesh) = self.meshes.get(mesh_id) {
            return Some(Cow::Borrowed(mesh));
        }
        self.terrain_surfaces
            .iter()
            .find(|surface| surface.mesh_id == mesh_id)
            .map(|surface| Cow::Owned(surface.generate_mesh(time_seconds)))
    }

    pub fn ground_height_at(&self, world_position: Vec3, time_seconds: f32) -> Option<f32> {
        self.terrain_surfaces
            .iter()
            .filter_map(|surface| surface.height_at_world_position(world_position, time_seconds))
            .max_by(|left, right| left.partial_cmp(right).unwrap_or(std::cmp::Ordering::Equal))
    }
}

impl TerrainSurface {
    pub fn height_at_world_position(&self, world_position: Vec3, time_seconds: f32) -> Option<f32> {
        let half_extent_x = self.half_extents.x.max(0.001);
        let half_extent_z = self.half_extents.z.max(0.001);
        let local_x = (world_position.x - self.center.x) / half_extent_x;
        let local_z = (world_position.z - self.center.z) / half_extent_z;
        if local_x.abs() > 1.0 || local_z.abs() > 1.0 {
            return None;
        }
        Some(self.center.y + sample_terrain_height(self, local_x, local_z, time_seconds))
    }

    pub fn generate_mesh(&self, time_seconds: f32) -> Mesh {
        let segments_x = self.resolution[0].max(2);
        let segments_z = self.resolution[1].max(2);
        let mut heights = vec![0.0; segments_x * segments_z];
        let index_of = |x: usize, z: usize| -> usize { z * segments_x + x };

        for z in 0..segments_z {
            let tz = z as f32 / (segments_z - 1) as f32;
            let local_z = tz * 2.0 - 1.0;
            for x in 0..segments_x {
                let tx = x as f32 / (segments_x - 1) as f32;
                let local_x = tx * 2.0 - 1.0;
                heights[index_of(x, z)] =
                    sample_terrain_height(self, local_x, local_z, time_seconds);
            }
        }

        let mut vertices = Vec::with_capacity(segments_x * segments_z);
        for z in 0..segments_z {
            let tz = z as f32 / (segments_z - 1) as f32;
            let local_z = tz * 2.0 - 1.0;
            for x in 0..segments_x {
                let tx = x as f32 / (segments_x - 1) as f32;
                let local_x = tx * 2.0 - 1.0;
                let world_x = self.center.x + local_x * self.half_extents.x;
                let world_z = self.center.z + local_z * self.half_extents.z;
                let world_y = self.center.y + heights[index_of(x, z)];

                let left = heights[index_of(x.saturating_sub(1), z)];
                let right = heights[index_of((x + 1).min(segments_x - 1), z)];
                let down = heights[index_of(x, z.saturating_sub(1))];
                let up = heights[index_of(x, (z + 1).min(segments_z - 1))];
                let tangent_x = Vec3::new(
                    2.0 * self.half_extents.x / (segments_x - 1) as f32,
                    right - left,
                    0.0,
                );
                let tangent_z = Vec3::new(
                    0.0,
                    up - down,
                    2.0 * self.half_extents.z / (segments_z - 1) as f32,
                );
                let normal = tangent_z.cross(tangent_x).normalize();

                vertices.push(Vertex {
                    position: Vec3::new(world_x, world_y, world_z),
                    normal,
                });
            }
        }

        let mut triangles = Vec::with_capacity((segments_x - 1) * (segments_z - 1) * 2);
        for z in 0..segments_z - 1 {
            for x in 0..segments_x - 1 {
                let a = index_of(x, z);
                let b = index_of(x + 1, z);
                let c = index_of(x + 1, z + 1);
                let d = index_of(x, z + 1);
                triangles.push([a, b, c]);
                triangles.push([a, c, d]);
            }
        }

        Mesh {
            vertices,
            triangles,
        }
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct SceneCatalog {
    pub default_scene: String,
    pub scenes: BTreeMap<String, SceneDescription>,
    pub scene_aliases: BTreeMap<String, String>,
}

impl Default for SceneCatalog {
    fn default() -> Self {
        let dcc_suite_scene = build_dcc_suite_scene();
        let tensor_stream_probe = build_tensor_stream_probe_scene();
        let luminous_port = build_luminous_port_scene();
        let magma_terraces = build_magma_terraces_scene();
        let material_atrium = build_material_atrium_scene();
        let retirement_demo = build_retirement_demo_scene();
        let kerr_black_hole = build_kerr_black_hole_scene();
        let mut scenes = BTreeMap::new();
        scenes.insert(dcc_suite_scene.name.clone(), dcc_suite_scene);
        scenes.insert(tensor_stream_probe.name.clone(), tensor_stream_probe);
        scenes.insert(luminous_port.name.clone(), luminous_port);
        scenes.insert(magma_terraces.name.clone(), magma_terraces);
        scenes.insert(material_atrium.name.clone(), material_atrium);
        scenes.insert(retirement_demo.name.clone(), retirement_demo);
        scenes.insert(kerr_black_hole.name.clone(), kerr_black_hole);
        let scene_aliases = BTreeMap::from([
            (
                "gpu_compute_surface_probe".to_string(),
                "tensor_stream_probe".to_string(),
            ),
            (
                "spv_ui_surface_probe".to_string(),
                "tensor_stream_probe".to_string(),
            ),
            ("starforge".to_string(), "luminous_port".to_string()),
            ("emberfall".to_string(), "magma_terraces".to_string()),
            (
                "renderer_atrium".to_string(),
                "material_atrium".to_string(),
            ),
            (
                "material_gallery".to_string(),
                "material_atrium".to_string(),
            ),
            (
                "dcc_authoring_startup".to_string(),
                "dcc_suite_scene".to_string(),
            ),
            (
                "ui_surface_probe".to_string(),
                "tensor_stream_probe".to_string(),
            ),
        ]);

        Self {
            default_scene: "luminous_port".to_string(),
            scenes,
            scene_aliases,
        }
    }
}

impl SceneCatalog {
    pub fn scene(&self, name: &str) -> Option<&SceneDescription> {
        self.scenes
            .get(name)
            .or_else(|| {
                self.scene_aliases
                    .get(name)
                    .and_then(|canonical| self.scenes.get(canonical))
            })
            .or_else(|| self.scenes.get(&self.default_scene))
    }
}

fn build_dcc_suite_scene() -> SceneDescription {
    let mut meshes = BTreeMap::new();
    meshes.insert("cube".to_string(), mesh_cube());
    meshes.insert("floor".to_string(), mesh_plane());

    let mut materials = BTreeMap::new();
    materials.insert(
        "startup_cube_default".to_string(),
        Material {
            base_color: ColorRgb::new(0.73, 0.74, 0.78),
            specular_color: ColorRgb::new(0.98, 0.99, 1.0),
            ambient_strength: 0.20,
            diffuse_strength: 1.0,
            specular_strength: 0.72,
            shininess: 52.0,
        },
    );
    materials.insert(
        "studio_floor".to_string(),
        Material {
            base_color: ColorRgb::new(0.18, 0.19, 0.22),
            specular_color: ColorRgb::new(0.42, 0.44, 0.50),
            ambient_strength: 0.24,
            diffuse_strength: 0.82,
            specular_strength: 0.14,
            shininess: 10.0,
        },
    );
    materials.insert(
        "studio_backdrop".to_string(),
        Material {
            base_color: ColorRgb::new(0.26, 0.27, 0.31),
            specular_color: ColorRgb::new(0.56, 0.58, 0.64),
            ambient_strength: 0.22,
            diffuse_strength: 0.76,
            specular_strength: 0.10,
            shininess: 12.0,
        },
    );

    SceneDescription {
        name: "dcc_suite_scene".to_string(),
        viewport_summary:
            "dcc startup scene | default blender cube | studio clay authoring light rig".to_string(),
        background: BackgroundGradient {
            top: ColorRgb::new(0.16, 0.18, 0.23),
            bottom: ColorRgb::new(0.05, 0.06, 0.08),
        },
        camera: Camera {
            target: Vec3::new(0.0, 0.15, 0.0),
            up: Vec3::UP,
            orbit_radius: 7.8,
            orbit_height: 3.1,
            orbit_speed_radians_per_second: 0.0,
            fov_y_degrees: 42.0,
            near_plane: 0.05,
            far_plane: 140.0,
        },
        lighting: LightingRig {
            ambient_color: ColorRgb::new(0.78, 0.82, 0.92),
            ambient_intensity: 0.22,
            directional_lights: vec![
                DirectionalLight {
                    direction: Vec3::new(-0.44, -1.0, -0.28).normalize(),
                    color: ColorRgb::new(1.0, 0.97, 0.92),
                    intensity: 1.18,
                },
                DirectionalLight {
                    direction: Vec3::new(0.62, -0.55, 0.42).normalize(),
                    color: ColorRgb::new(0.44, 0.58, 0.96),
                    intensity: 0.34,
                },
            ],
            point_lights: vec![
                PointLight {
                    position: Vec3::new(3.2, 3.8, 3.1),
                    color: ColorRgb::new(1.0, 0.78, 0.58),
                    intensity: 1.05,
                    range: 12.0,
                },
                PointLight {
                    position: Vec3::new(-3.8, 2.4, -4.2),
                    color: ColorRgb::new(0.46, 0.72, 1.0),
                    intensity: 0.88,
                    range: 15.0,
                },
                PointLight {
                    position: Vec3::new(0.0, 5.2, -1.6),
                    color: ColorRgb::new(0.92, 0.96, 1.0),
                    intensity: 0.46,
                    range: 14.0,
                },
            ],
        },
        meshes,
        materials,
        instances: vec![
            SceneInstance {
                id: "blender_startup_cube".to_string(),
                mesh: "cube".to_string(),
                material: "startup_cube_default".to_string(),
                transform: Transform::identity(),
            },
            SceneInstance {
                id: "studio_floor".to_string(),
                mesh: "floor".to_string(),
                material: "studio_floor".to_string(),
                transform: Transform::identity()
                    .with_translation(Vec3::new(0.0, -1.0, 0.0))
                    .with_scale(Vec3::new(7.5, 1.0, 7.5)),
            },
            SceneInstance {
                id: "studio_backdrop".to_string(),
                mesh: "floor".to_string(),
                material: "studio_backdrop".to_string(),
                transform: Transform::identity()
                    .with_translation(Vec3::new(0.0, 2.35, -6.2))
                    .with_rotation(Vec3::new(-1.5707964, 0.0, 0.0))
                    .with_scale(Vec3::new(7.5, 1.0, 4.6)),
            },
        ],
        animations: Vec::new(),
        particle_emitters: Vec::new(),
        black_hole: None,
        terrain_surfaces: Vec::new(),
    }
}

fn build_tensor_stream_probe_scene() -> SceneDescription {
    let mut meshes = BTreeMap::new();
    meshes.insert("cube".to_string(), mesh_cube());
    meshes.insert("floor".to_string(), mesh_plane());
    meshes.insert("orb".to_string(), mesh_uv_sphere(6, 12));
    meshes.insert("pyramid".to_string(), mesh_pyramid());

    let mut materials = BTreeMap::new();
    materials.insert(
        "dock".to_string(),
        Material {
            base_color: ColorRgb::new(0.08, 0.11, 0.17),
            specular_color: ColorRgb::new(0.36, 0.48, 0.70),
            ambient_strength: 0.32,
            diffuse_strength: 0.84,
            specular_strength: 0.20,
            shininess: 12.0,
        },
    );
    materials.insert(
        "pulse_core".to_string(),
        Material {
            base_color: ColorRgb::new(0.10, 0.78, 1.0),
            specular_color: ColorRgb::new(0.92, 0.98, 1.0),
            ambient_strength: 0.20,
            diffuse_strength: 0.98,
            specular_strength: 0.72,
            shininess: 34.0,
        },
    );
    materials.insert(
        "signal".to_string(),
        Material {
            base_color: ColorRgb::new(1.0, 0.82, 0.28),
            specular_color: ColorRgb::new(1.0, 0.96, 0.72),
            ambient_strength: 0.16,
            diffuse_strength: 0.96,
            specular_strength: 0.36,
            shininess: 20.0,
        },
    );
    materials.insert(
        "relay".to_string(),
        Material {
            base_color: ColorRgb::new(0.24, 0.34, 0.54),
            specular_color: ColorRgb::new(0.62, 0.76, 0.96),
            ambient_strength: 0.24,
            diffuse_strength: 0.90,
            specular_strength: 0.32,
            shininess: 18.0,
        },
    );

    SceneDescription {
        name: "tensor_stream_probe".to_string(),
        viewport_summary: "tensor stream probe | compute relay deck | spv runtime preview"
            .to_string(),
        background: BackgroundGradient {
            top: ColorRgb::new(0.03, 0.08, 0.14),
            bottom: ColorRgb::new(0.01, 0.02, 0.06),
        },
        camera: Camera {
            target: Vec3::new(0.0, 0.4, 0.0),
            up: Vec3::UP,
            orbit_radius: 7.4,
            orbit_height: 2.2,
            orbit_speed_radians_per_second: 0.28,
            fov_y_degrees: 50.0,
            near_plane: 0.05,
            far_plane: 140.0,
        },
        lighting: LightingRig {
            ambient_color: ColorRgb::new(0.74, 0.82, 1.0),
            ambient_intensity: 0.30,
            directional_lights: vec![
                DirectionalLight {
                    direction: Vec3::new(-0.35, -1.0, -0.42).normalize(),
                    color: ColorRgb::new(0.74, 0.86, 1.0),
                    intensity: 1.05,
                },
                DirectionalLight {
                    direction: Vec3::new(0.58, -0.32, 0.26).normalize(),
                    color: ColorRgb::new(0.20, 0.76, 1.0),
                    intensity: 0.44,
                },
            ],
            point_lights: vec![
                PointLight {
                    position: Vec3::new(0.0, 2.0, 0.0),
                    color: ColorRgb::new(0.18, 0.86, 1.0),
                    intensity: 1.45,
                    range: 10.0,
                },
                PointLight {
                    position: Vec3::new(-2.8, 1.0, 1.6),
                    color: ColorRgb::new(1.0, 0.82, 0.30),
                    intensity: 0.88,
                    range: 9.0,
                },
                PointLight {
                    position: Vec3::new(2.8, 1.1, -1.8),
                    color: ColorRgb::new(0.52, 0.72, 1.0),
                    intensity: 0.62,
                    range: 9.0,
                },
            ],
        },
        meshes,
        materials,
        instances: vec![
            SceneInstance {
                id: "relay_deck".to_string(),
                mesh: "floor".to_string(),
                material: "dock".to_string(),
                transform: Transform::identity()
                    .with_translation(Vec3::new(0.0, -1.0, 0.0))
                    .with_scale(Vec3::new(6.0, 1.0, 6.0)),
            },
            SceneInstance {
                id: "pulse_core".to_string(),
                mesh: "orb".to_string(),
                material: "pulse_core".to_string(),
                transform: Transform::identity()
                    .with_translation(Vec3::new(0.0, 0.75, 0.0))
                    .with_scale(Vec3::new(0.92, 0.92, 0.92)),
            },
            SceneInstance {
                id: "signal_north".to_string(),
                mesh: "pyramid".to_string(),
                material: "signal".to_string(),
                transform: Transform::identity()
                    .with_translation(Vec3::new(0.0, 0.12, -2.4))
                    .with_scale(Vec3::new(0.74, 1.55, 0.74)),
            },
            SceneInstance {
                id: "signal_south".to_string(),
                mesh: "pyramid".to_string(),
                material: "signal".to_string(),
                transform: Transform::identity()
                    .with_translation(Vec3::new(0.0, 0.12, 2.4))
                    .with_rotation(Vec3::new(0.0, 1.57, 0.0))
                    .with_scale(Vec3::new(0.74, 1.55, 0.74)),
            },
            SceneInstance {
                id: "relay_east".to_string(),
                mesh: "cube".to_string(),
                material: "relay".to_string(),
                transform: Transform::identity()
                    .with_translation(Vec3::new(2.7, 0.2, 0.0))
                    .with_rotation(Vec3::new(0.08, 0.34, 0.0))
                    .with_scale(Vec3::new(0.48, 1.18, 0.48)),
            },
            SceneInstance {
                id: "relay_west".to_string(),
                mesh: "cube".to_string(),
                material: "relay".to_string(),
                transform: Transform::identity()
                    .with_translation(Vec3::new(-2.7, 0.2, 0.0))
                    .with_rotation(Vec3::new(0.08, -0.34, 0.0))
                    .with_scale(Vec3::new(0.48, 1.18, 0.48)),
            },
        ],
        animations: vec![
            SceneAnimation::Spin {
                instance_id: "pulse_core".to_string(),
                axis_radians_per_second: Vec3::new(0.0, 0.95, 0.10),
            },
            SceneAnimation::Bob {
                instance_id: "pulse_core".to_string(),
                amplitude: 0.24,
                speed_radians_per_second: 1.6,
            },
            SceneAnimation::Spin {
                instance_id: "relay_east".to_string(),
                axis_radians_per_second: Vec3::new(0.0, 0.42, 0.04),
            },
            SceneAnimation::Spin {
                instance_id: "relay_west".to_string(),
                axis_radians_per_second: Vec3::new(0.0, -0.42, 0.04),
            },
        ],
        particle_emitters: vec![
            ParticleEmitter {
                id: "tensor_ring".to_string(),
                center: Vec3::new(0.0, 0.8, 0.0),
                axis: Vec3::UP,
                radial_range: [1.4, 3.6],
                vertical_range: [-0.18, 0.24],
                particle_size_range: [0.04, 0.12],
                particle_count: 24,
                orbit_radians_per_second: 0.92,
                swirl: 0.42,
                drift: Vec3::new(0.0, 0.18, 0.0),
                color_start: ColorRgb::new(0.24, 0.88, 1.0),
                color_end: ColorRgb::new(1.0, 0.84, 0.32),
                emissive_strength: 0.52,
                softness: 1.25,
                depth_test: false,
            },
            ParticleEmitter {
                id: "dispatch_lane".to_string(),
                center: Vec3::new(0.0, 0.24, 0.0),
                axis: Vec3::new(0.0, 0.15, 1.0).normalize(),
                radial_range: [2.4, 4.9],
                vertical_range: [-0.10, 0.28],
                particle_size_range: [0.03, 0.08],
                particle_count: 18,
                orbit_radians_per_second: -0.68,
                swirl: 0.20,
                drift: Vec3::new(0.0, 0.06, 0.0),
                color_start: ColorRgb::new(0.12, 0.58, 1.0),
                color_end: ColorRgb::new(0.76, 0.92, 1.0),
                emissive_strength: 0.38,
                softness: 1.0,
                depth_test: true,
            },
        ],
        black_hole: None,
        terrain_surfaces: Vec::new(),
    }
}

fn build_magma_terraces_scene() -> SceneDescription {
    let mut meshes = BTreeMap::new();
    meshes.insert("cube".to_string(), mesh_cube());
    meshes.insert("floor".to_string(), mesh_plane());
    meshes.insert("pyramid".to_string(), mesh_pyramid());
    meshes.insert("orb".to_string(), mesh_uv_sphere(8, 16));

    let mut materials = BTreeMap::new();
    materials.insert(
        "basalt".to_string(),
        Material {
            base_color: ColorRgb::new(0.10, 0.10, 0.12),
            specular_color: ColorRgb::new(0.42, 0.38, 0.44),
            ambient_strength: 0.34,
            diffuse_strength: 0.92,
            specular_strength: 0.14,
            shininess: 10.0,
        },
    );
    materials.insert(
        "ash".to_string(),
        Material {
            base_color: ColorRgb::new(0.25, 0.23, 0.22),
            specular_color: ColorRgb::new(0.52, 0.48, 0.42),
            ambient_strength: 0.30,
            diffuse_strength: 0.86,
            specular_strength: 0.14,
            shininess: 12.0,
        },
    );
    materials.insert(
        "magma".to_string(),
        Material {
            base_color: ColorRgb::new(1.0, 0.34, 0.06),
            specular_color: ColorRgb::new(1.0, 0.88, 0.54),
            ambient_strength: 0.28,
            diffuse_strength: 1.0,
            specular_strength: 0.58,
            shininess: 28.0,
        },
    );
    materials.insert(
        "crust".to_string(),
        Material {
            base_color: ColorRgb::new(0.52, 0.17, 0.08),
            specular_color: ColorRgb::new(0.94, 0.48, 0.16),
            ambient_strength: 0.24,
            diffuse_strength: 0.96,
            specular_strength: 0.28,
            shininess: 16.0,
        },
    );
    materials.insert(
        "sulfur".to_string(),
        Material {
            base_color: ColorRgb::new(0.97, 0.79, 0.22),
            specular_color: ColorRgb::new(1.0, 0.95, 0.62),
            ambient_strength: 0.18,
            diffuse_strength: 0.96,
            specular_strength: 0.26,
            shininess: 18.0,
        },
    );
    materials.insert(
        "obsidian".to_string(),
        Material {
            base_color: ColorRgb::new(0.08, 0.06, 0.12),
            specular_color: ColorRgb::new(0.72, 0.62, 0.86),
            ambient_strength: 0.20,
            diffuse_strength: 0.76,
            specular_strength: 0.72,
            shininess: 34.0,
        },
    );
    materials.insert(
        "emberglass".to_string(),
        Material {
            base_color: ColorRgb::new(1.0, 0.54, 0.18),
            specular_color: ColorRgb::new(1.0, 0.92, 0.70),
            ambient_strength: 0.22,
            diffuse_strength: 1.0,
            specular_strength: 0.76,
            shininess: 38.0,
        },
    );
    materials.insert(
        "slag".to_string(),
        Material {
            base_color: ColorRgb::new(0.20, 0.14, 0.14),
            specular_color: ColorRgb::new(0.60, 0.36, 0.32),
            ambient_strength: 0.26,
            diffuse_strength: 0.88,
            specular_strength: 0.22,
            shininess: 14.0,
        },
    );

    let mut instances = Vec::new();
    instances.push(SceneInstance {
        id: "terrain_body".to_string(),
        mesh: "terrain_heightfield".to_string(),
        material: "ash".to_string(),
        transform: Transform::identity(),
    });
    instances.push(SceneInstance {
        id: "magma_lake".to_string(),
        mesh: "floor".to_string(),
        material: "magma".to_string(),
        transform: Transform::identity()
            .with_translation(Vec3::new(0.0, -0.42, 0.0))
            .with_scale(Vec3::new(4.8, 1.0, 4.1)),
    });

    let ring_columns = [
        (11.8, -1.3, 0.88, 1.95, 30usize, "ash"),
        (8.4, -0.4, 0.72, 1.55, 22usize, "basalt"),
        (5.8, 0.5, 0.56, 1.18, 16usize, "obsidian"),
    ];
    for (radius, y, column_scale, height_scale, count, material) in ring_columns {
        for index in 0..count {
            let angle = index as f32 / count as f32 * std::f32::consts::TAU;
            let radial_wave = 0.78 + ((index % 5) as f32 * 0.07);
            instances.push(SceneInstance {
                id: format!("{material}_column_{count}_{index}"),
                mesh: "cube".to_string(),
                material: material.to_string(),
                transform: Transform::identity()
                    .with_translation(Vec3::new(
                        angle.cos() * radius,
                        y + ((index % 3) as f32 * 0.22),
                        angle.sin() * radius,
                    ))
                    .with_rotation(Vec3::new(0.04, angle, 0.03))
                    .with_scale(Vec3::new(
                        column_scale * radial_wave,
                        height_scale + ((index % 6) as f32 * 0.26),
                        column_scale * radial_wave,
                    )),
            });
        }
    }

    for arc_index in 0..18usize {
        let t = arc_index as f32 / 18.0;
        let angle = t * std::f32::consts::TAU;
        let radius = 15.5 + (arc_index % 4) as f32 * 1.2;
        instances.push(SceneInstance {
            id: format!("rim_spire_{arc_index}"),
            mesh: "pyramid".to_string(),
            material: if arc_index % 3 == 0 {
                "obsidian".to_string()
            } else {
                "slag".to_string()
            },
            transform: Transform::identity()
                .with_translation(Vec3::new(
                    angle.cos() * radius,
                    -0.4 + (arc_index % 3) as f32 * 0.24,
                    angle.sin() * radius,
                ))
                .with_rotation(Vec3::new(0.0, angle + 0.25, 0.0))
                .with_scale(Vec3::new(
                    0.86 + (arc_index % 2) as f32 * 0.18,
                    2.6 + (arc_index % 5) as f32 * 0.35,
                    0.86 + (arc_index % 2) as f32 * 0.18,
                )),
        });
    }

    let vent_specs = [
        (
            "vent_north",
            Vec3::new(0.0, 1.42, -2.6),
            Vec3::new(0.62, 2.0, 0.62),
        ),
        (
            "vent_south",
            Vec3::new(0.0, 1.34, 2.7),
            Vec3::new(0.58, 1.9, 0.58),
        ),
        (
            "vent_east",
            Vec3::new(2.7, 1.18, 0.2),
            Vec3::new(0.52, 1.72, 0.52),
        ),
        (
            "vent_west",
            Vec3::new(-2.6, 1.14, -0.18),
            Vec3::new(0.50, 1.68, 0.50),
        ),
        (
            "vent_northeast",
            Vec3::new(2.1, 1.28, -2.2),
            Vec3::new(0.40, 1.48, 0.40),
        ),
        (
            "vent_southwest",
            Vec3::new(-2.2, 1.22, 2.1),
            Vec3::new(0.42, 1.42, 0.42),
        ),
    ];
    for (id, translation, scale) in vent_specs {
        instances.push(SceneInstance {
            id: id.to_string(),
            mesh: "pyramid".to_string(),
            material: "crust".to_string(),
            transform: Transform::identity()
                .with_translation(translation)
                .with_scale(scale),
        });
    }

    let terrace_blocks = [
        (
            "north_plate",
            Vec3::new(0.0, 0.55, -5.8),
            Vec3::new(2.6, 0.44, 1.2),
            "ash",
        ),
        (
            "south_plate",
            Vec3::new(0.0, 0.42, 5.6),
            Vec3::new(2.4, 0.40, 1.1),
            "ash",
        ),
        (
            "east_shelf",
            Vec3::new(5.7, -0.12, 0.0),
            Vec3::new(1.22, 0.54, 2.8),
            "basalt",
        ),
        (
            "west_shelf",
            Vec3::new(-5.6, -0.06, 0.0),
            Vec3::new(1.28, 0.60, 2.7),
            "basalt",
        ),
        (
            "magma_bridge_north",
            Vec3::new(0.0, 1.42, -1.0),
            Vec3::new(0.54, 0.08, 3.0),
            "magma",
        ),
        (
            "magma_bridge_south",
            Vec3::new(0.0, 1.38, 1.1),
            Vec3::new(0.46, 0.08, 2.8),
            "emberglass",
        ),
        (
            "central_dais",
            Vec3::new(0.0, 1.18, 0.0),
            Vec3::new(1.6, 0.22, 1.6),
            "obsidian",
        ),
    ];
    for (id, translation, scale, material) in terrace_blocks {
        instances.push(SceneInstance {
            id: id.to_string(),
            mesh: "cube".to_string(),
            material: material.to_string(),
            transform: Transform::identity()
                .with_translation(translation)
                .with_scale(scale),
        });
    }

    for row in 0..4usize {
        let z = -9.0 + row as f32 * 6.0;
        for column in 0..5usize {
            let x = -10.0 + column as f32 * 5.0;
            if x.abs() < 2.5 && z.abs() < 2.5 {
                continue;
            }
            instances.push(SceneInstance {
                id: format!("terrace_slab_{row}_{column}"),
                mesh: "cube".to_string(),
                material: if (row + column) % 2 == 0 {
                    "ash".to_string()
                } else {
                    "slag".to_string()
                },
                transform: Transform::identity()
                    .with_translation(Vec3::new(x, -0.55 + ((row + column) % 3) as f32 * 0.22, z))
                    .with_rotation(Vec3::new(0.0, (row as f32 - column as f32) * 0.08, 0.0))
                    .with_scale(Vec3::new(
                        1.8 + (column % 2) as f32 * 0.4,
                        0.28 + (row % 2) as f32 * 0.10,
                        1.4 + (row % 3) as f32 * 0.28,
                    )),
            });
        }
    }

    for bridge_index in 0..6usize {
        let angle = bridge_index as f32 / 6.0 * std::f32::consts::TAU;
        let translation = Vec3::new(angle.cos() * 7.8, 0.84, angle.sin() * 7.8);
        instances.push(SceneInstance {
            id: format!("bridge_pylon_{bridge_index}"),
            mesh: "cube".to_string(),
            material: "obsidian".to_string(),
            transform: Transform::identity()
                .with_translation(translation)
                .with_rotation(Vec3::new(0.04, angle, 0.0))
                .with_scale(Vec3::new(0.42, 2.1, 0.42)),
        });
        instances.push(SceneInstance {
            id: format!("bridge_cap_{bridge_index}"),
            mesh: "orb".to_string(),
            material: "emberglass".to_string(),
            transform: Transform::identity()
                .with_translation(translation + Vec3::new(0.0, 1.65, 0.0))
                .with_scale(Vec3::new(0.22, 0.22, 0.22)),
        });
    }

    for debris_index in 0..24usize {
        let angle = debris_index as f32 / 24.0 * std::f32::consts::TAU;
        let radius = 6.4 + (debris_index % 7) as f32 * 1.55;
        instances.push(SceneInstance {
            id: format!("debris_cluster_{debris_index}"),
            mesh: if debris_index % 3 == 0 {
                "pyramid".to_string()
            } else {
                "cube".to_string()
            },
            material: if debris_index % 5 == 0 {
                "crust".to_string()
            } else {
                "slag".to_string()
            },
            transform: Transform::identity()
                .with_translation(Vec3::new(
                    angle.cos() * radius,
                    0.12 + (debris_index % 4) as f32 * 0.18,
                    angle.sin() * radius,
                ))
                .with_rotation(Vec3::new(
                    debris_index as f32 * 0.12,
                    angle * 1.4,
                    debris_index as f32 * 0.05,
                ))
                .with_scale(Vec3::new(
                    0.28 + (debris_index % 3) as f32 * 0.12,
                    0.28 + (debris_index % 5) as f32 * 0.18,
                    0.28 + (debris_index % 4) as f32 * 0.10,
                )),
        });
    }

    let orb_specs = [
        (
            "magma_core",
            Vec3::new(0.0, 2.6, 0.0),
            Vec3::new(0.78, 0.78, 0.78),
            "magma",
        ),
        (
            "sulfur_beacon_a",
            Vec3::new(-3.2, 2.3, 3.5),
            Vec3::new(0.34, 0.34, 0.34),
            "sulfur",
        ),
        (
            "sulfur_beacon_b",
            Vec3::new(3.4, 2.6, -3.1),
            Vec3::new(0.38, 0.38, 0.38),
            "sulfur",
        ),
        (
            "obsidian_eye",
            Vec3::new(0.0, 3.5, -0.2),
            Vec3::new(0.28, 0.28, 0.28),
            "obsidian",
        ),
        (
            "ember_satellite_a",
            Vec3::new(-5.0, 1.9, -1.2),
            Vec3::new(0.24, 0.24, 0.24),
            "emberglass",
        ),
        (
            "ember_satellite_b",
            Vec3::new(4.8, 1.7, 1.6),
            Vec3::new(0.26, 0.26, 0.26),
            "emberglass",
        ),
    ];
    for (id, translation, scale, material) in orb_specs {
        instances.push(SceneInstance {
            id: id.to_string(),
            mesh: "orb".to_string(),
            material: material.to_string(),
            transform: Transform::identity()
                .with_translation(translation)
                .with_scale(scale),
        });
    }

    SceneDescription {
        name: "magma_terraces".to_string(),
        viewport_summary:
            "megastructure caldera | ember bridges | dense ashfall | ctrl-drag gizmo editing"
                .to_string(),
        background: BackgroundGradient {
            top: ColorRgb::new(0.03, 0.05, 0.08),
            bottom: ColorRgb::new(0.30, 0.09, 0.03),
        },
        camera: Camera {
            target: Vec3::new(0.0, 1.5, 0.0),
            up: Vec3::UP,
            orbit_radius: 24.0,
            orbit_height: 8.4,
            orbit_speed_radians_per_second: 0.10,
            fov_y_degrees: 58.0,
            near_plane: 0.1,
            far_plane: 260.0,
        },
        lighting: LightingRig {
            ambient_color: ColorRgb::new(0.96, 0.54, 0.30),
            ambient_intensity: 0.30,
            directional_lights: vec![
                DirectionalLight {
                    direction: Vec3::new(-0.40, -1.0, -0.24).normalize(),
                    color: ColorRgb::new(1.0, 0.82, 0.60),
                    intensity: 1.18,
                },
                DirectionalLight {
                    direction: Vec3::new(0.42, -0.48, 0.36).normalize(),
                    color: ColorRgb::new(0.20, 0.34, 0.72),
                    intensity: 0.34,
                },
            ],
            point_lights: vec![
                PointLight {
                    position: Vec3::new(0.0, 3.0, 0.0),
                    color: ColorRgb::new(1.0, 0.48, 0.14),
                    intensity: 3.6,
                    range: 16.0,
                },
                PointLight {
                    position: Vec3::new(0.0, 1.9, -2.6),
                    color: ColorRgb::new(1.0, 0.58, 0.18),
                    intensity: 2.1,
                    range: 10.0,
                },
                PointLight {
                    position: Vec3::new(0.0, 1.9, 2.7),
                    color: ColorRgb::new(1.0, 0.56, 0.18),
                    intensity: 2.0,
                    range: 10.0,
                },
                PointLight {
                    position: Vec3::new(2.7, 1.8, 0.2),
                    color: ColorRgb::new(1.0, 0.62, 0.22),
                    intensity: 1.7,
                    range: 8.8,
                },
                PointLight {
                    position: Vec3::new(-2.6, 1.8, -0.2),
                    color: ColorRgb::new(1.0, 0.60, 0.22),
                    intensity: 1.6,
                    range: 8.8,
                },
                PointLight {
                    position: Vec3::new(3.4, 2.6, -3.1),
                    color: ColorRgb::new(0.98, 0.82, 0.26),
                    intensity: 1.2,
                    range: 8.4,
                },
                PointLight {
                    position: Vec3::new(-3.2, 2.3, 3.5),
                    color: ColorRgb::new(0.98, 0.78, 0.24),
                    intensity: 1.1,
                    range: 8.4,
                },
                PointLight {
                    position: Vec3::new(0.0, 4.4, -0.2),
                    color: ColorRgb::new(0.44, 0.56, 1.0),
                    intensity: 0.72,
                    range: 12.0,
                },
            ],
        },
        meshes,
        materials,
        instances,
        animations: vec![
            SceneAnimation::Bob {
                instance_id: "magma_core".to_string(),
                amplitude: 0.46,
                speed_radians_per_second: 1.4,
            },
            SceneAnimation::Spin {
                instance_id: "magma_core".to_string(),
                axis_radians_per_second: Vec3::new(0.0, 0.92, 0.08),
            },
            SceneAnimation::Bob {
                instance_id: "sulfur_beacon_a".to_string(),
                amplitude: 0.22,
                speed_radians_per_second: 2.0,
            },
            SceneAnimation::Bob {
                instance_id: "sulfur_beacon_b".to_string(),
                amplitude: 0.24,
                speed_radians_per_second: 1.7,
            },
            SceneAnimation::Spin {
                instance_id: "obsidian_eye".to_string(),
                axis_radians_per_second: Vec3::new(0.20, -1.18, 0.10),
            },
            SceneAnimation::Spin {
                instance_id: "ember_satellite_a".to_string(),
                axis_radians_per_second: Vec3::new(0.14, 1.24, 0.12),
            },
            SceneAnimation::Spin {
                instance_id: "ember_satellite_b".to_string(),
                axis_radians_per_second: Vec3::new(0.10, -1.18, 0.15),
            },
        ],
        particle_emitters: vec![
            ParticleEmitter {
                id: "caldera_plume".to_string(),
                center: Vec3::new(0.0, 1.34, 0.0),
                axis: Vec3::UP,
                radial_range: [0.18, 1.80],
                vertical_range: [-0.08, 0.32],
                particle_size_range: [0.10, 0.34],
                particle_count: 220,
                orbit_radians_per_second: 1.62,
                swirl: 0.92,
                drift: Vec3::new(0.0, 3.4, 0.0),
                color_start: ColorRgb::new(1.0, 0.38, 0.08),
                color_end: ColorRgb::new(1.0, 0.78, 0.30),
                emissive_strength: 1.18,
                softness: 1.6,
                depth_test: true,
            },
            ParticleEmitter {
                id: "north_vent_spray".to_string(),
                center: Vec3::new(0.0, 1.48, -2.6),
                axis: Vec3::UP,
                radial_range: [0.10, 1.10],
                vertical_range: [-0.05, 0.22],
                particle_size_range: [0.08, 0.22],
                particle_count: 110,
                orbit_radians_per_second: 2.1,
                swirl: 0.72,
                drift: Vec3::new(0.0, 2.4, 0.0),
                color_start: ColorRgb::new(1.0, 0.34, 0.10),
                color_end: ColorRgb::new(1.0, 0.76, 0.38),
                emissive_strength: 0.94,
                softness: 1.3,
                depth_test: true,
            },
            ParticleEmitter {
                id: "south_vent_spray".to_string(),
                center: Vec3::new(0.0, 1.42, 2.7),
                axis: Vec3::UP,
                radial_range: [0.10, 1.00],
                vertical_range: [-0.05, 0.22],
                particle_size_range: [0.08, 0.22],
                particle_count: 104,
                orbit_radians_per_second: -1.9,
                swirl: 0.74,
                drift: Vec3::new(0.0, 2.3, 0.0),
                color_start: ColorRgb::new(1.0, 0.38, 0.12),
                color_end: ColorRgb::new(1.0, 0.72, 0.34),
                emissive_strength: 0.92,
                softness: 1.25,
                depth_test: true,
            },
            ParticleEmitter {
                id: "east_vent_spray".to_string(),
                center: Vec3::new(2.7, 1.24, 0.2),
                axis: Vec3::UP,
                radial_range: [0.08, 0.88],
                vertical_range: [-0.04, 0.16],
                particle_size_range: [0.07, 0.18],
                particle_count: 82,
                orbit_radians_per_second: 1.64,
                swirl: 0.64,
                drift: Vec3::new(0.0, 1.8, 0.0),
                color_start: ColorRgb::new(1.0, 0.34, 0.08),
                color_end: ColorRgb::new(1.0, 0.70, 0.28),
                emissive_strength: 0.82,
                softness: 1.18,
                depth_test: true,
            },
            ParticleEmitter {
                id: "west_vent_spray".to_string(),
                center: Vec3::new(-2.6, 1.18, -0.2),
                axis: Vec3::UP,
                radial_range: [0.08, 0.88],
                vertical_range: [-0.04, 0.16],
                particle_size_range: [0.07, 0.18],
                particle_count: 82,
                orbit_radians_per_second: -1.58,
                swirl: 0.62,
                drift: Vec3::new(0.0, 1.7, 0.0),
                color_start: ColorRgb::new(1.0, 0.34, 0.08),
                color_end: ColorRgb::new(1.0, 0.70, 0.28),
                emissive_strength: 0.82,
                softness: 1.18,
                depth_test: true,
            },
            ParticleEmitter {
                id: "ash_gravity".to_string(),
                center: Vec3::new(0.0, 9.6, 0.0),
                axis: Vec3::UP,
                radial_range: [4.0, 18.5],
                vertical_range: [-0.8, 0.8],
                particle_size_range: [0.04, 0.14],
                particle_count: 260,
                orbit_radians_per_second: 0.18,
                swirl: 0.26,
                drift: Vec3::new(0.0, -3.0, 0.0),
                color_start: ColorRgb::new(0.48, 0.44, 0.40),
                color_end: ColorRgb::new(0.20, 0.18, 0.18),
                emissive_strength: 0.08,
                softness: 1.4,
                depth_test: true,
            },
            ParticleEmitter {
                id: "magma_river".to_string(),
                center: Vec3::new(0.0, 1.42, 0.0),
                axis: Vec3::UP,
                radial_range: [0.4, 4.8],
                vertical_range: [-0.05, 0.05],
                particle_size_range: [0.06, 0.18],
                particle_count: 180,
                orbit_radians_per_second: 0.88,
                swirl: 1.05,
                drift: Vec3::new(0.0, 0.24, 0.0),
                color_start: ColorRgb::new(1.0, 0.30, 0.04),
                color_end: ColorRgb::new(1.0, 0.62, 0.18),
                emissive_strength: 0.96,
                softness: 1.2,
                depth_test: true,
            },
            ParticleEmitter {
                id: "ember_cinders".to_string(),
                center: Vec3::new(0.0, 4.8, 0.0),
                axis: Vec3::UP,
                radial_range: [2.0, 12.0],
                vertical_range: [-0.4, 0.8],
                particle_size_range: [0.03, 0.10],
                particle_count: 220,
                orbit_radians_per_second: 0.36,
                swirl: 0.48,
                drift: Vec3::new(0.0, 0.8, 0.0),
                color_start: ColorRgb::new(1.0, 0.50, 0.18),
                color_end: ColorRgb::new(1.0, 0.84, 0.36),
                emissive_strength: 0.44,
                softness: 1.0,
                depth_test: false,
            },
        ],
        black_hole: None,
        terrain_surfaces: vec![TerrainSurface {
            id: "terrace_caldera".to_string(),
            mesh_id: "terrain_heightfield".to_string(),
            center: Vec3::new(0.0, -1.9, 0.0),
            half_extents: Vec3::new(28.0, 0.0, 24.0),
            resolution: [156, 132],
            base_height: 0.0,
            height_amplitude: 1.92,
            terrace_step: 0.58,
            rim_strength: 3.8,
            ripple_amplitude: 0.24,
            ripple_frequency: 10.6,
            flow_speed: 0.84,
            caldera_radius: 0.28,
            caldera_depth: 3.1,
        }],
    }
}

fn build_luminous_port_scene() -> SceneDescription {
    let mut meshes = BTreeMap::new();
    meshes.insert("cube".to_string(), mesh_cube());
    meshes.insert("floor".to_string(), mesh_plane());
    meshes.insert("pyramid".to_string(), mesh_pyramid());
    meshes.insert("orb".to_string(), mesh_uv_sphere(6, 10));

    let mut materials = BTreeMap::new();
    materials.insert(
        "platform".to_string(),
        Material {
            base_color: ColorRgb::new(0.17, 0.21, 0.28),
            specular_color: ColorRgb::new(0.40, 0.48, 0.58),
            ambient_strength: 0.30,
            diffuse_strength: 0.88,
            specular_strength: 0.14,
            shininess: 10.0,
        },
    );
    materials.insert(
        "glass".to_string(),
        Material {
            base_color: ColorRgb::new(0.14, 0.82, 0.95),
            specular_color: ColorRgb::new(0.88, 0.98, 1.0),
            ambient_strength: 0.22,
            diffuse_strength: 0.96,
            specular_strength: 0.74,
            shininess: 34.0,
        },
    );
    materials.insert(
        "warm".to_string(),
        Material {
            base_color: ColorRgb::new(0.98, 0.64, 0.28),
            specular_color: ColorRgb::new(1.0, 0.92, 0.75),
            ambient_strength: 0.18,
            diffuse_strength: 0.95,
            specular_strength: 0.34,
            shininess: 18.0,
        },
    );
    materials.insert(
        "pearl".to_string(),
        Material {
            base_color: ColorRgb::new(0.80, 0.86, 0.96),
            specular_color: ColorRgb::new(1.0, 1.0, 1.0),
            ambient_strength: 0.26,
            diffuse_strength: 0.92,
            specular_strength: 0.48,
            shininess: 26.0,
        },
    );

    SceneDescription {
        name: "luminous_port".to_string(),
        viewport_summary: "luminous port | floating gallery | soft particles".to_string(),
        background: BackgroundGradient {
            top: ColorRgb::new(0.05, 0.11, 0.18),
            bottom: ColorRgb::new(0.12, 0.08, 0.15),
        },
        camera: Camera {
            target: Vec3::new(0.0, 0.6, 0.0),
            up: Vec3::UP,
            orbit_radius: 8.4,
            orbit_height: 2.7,
            orbit_speed_radians_per_second: 0.22,
            fov_y_degrees: 54.0,
            near_plane: 0.1,
            far_plane: 100.0,
        },
        lighting: LightingRig {
            ambient_color: ColorRgb::new(0.78, 0.84, 0.98),
            ambient_intensity: 0.36,
            directional_lights: vec![
                DirectionalLight {
                    direction: Vec3::new(-0.45, -1.0, -0.30).normalize(),
                    color: ColorRgb::new(0.92, 0.96, 1.0),
                    intensity: 1.10,
                },
                DirectionalLight {
                    direction: Vec3::new(0.55, -0.42, 0.35).normalize(),
                    color: ColorRgb::new(0.22, 0.56, 0.92),
                    intensity: 0.34,
                },
            ],
            point_lights: vec![
                PointLight {
                    position: Vec3::new(0.0, 2.8, 0.0),
                    color: ColorRgb::new(0.38, 0.92, 1.0),
                    intensity: 1.35,
                    range: 9.0,
                },
                PointLight {
                    position: Vec3::new(-2.4, 1.1, 2.6),
                    color: ColorRgb::new(1.0, 0.72, 0.42),
                    intensity: 0.92,
                    range: 8.0,
                },
                PointLight {
                    position: Vec3::new(2.5, 1.4, -2.4),
                    color: ColorRgb::new(0.58, 0.76, 1.0),
                    intensity: 0.72,
                    range: 8.0,
                },
            ],
        },
        meshes,
        materials,
        instances: vec![
            SceneInstance {
                id: "platform".to_string(),
                mesh: "floor".to_string(),
                material: "platform".to_string(),
                transform: Transform::identity()
                    .with_translation(Vec3::new(0.0, -1.0, 0.0))
                    .with_scale(Vec3::new(6.5, 1.0, 6.5)),
            },
            SceneInstance {
                id: "orb".to_string(),
                mesh: "orb".to_string(),
                material: "pearl".to_string(),
                transform: Transform::identity()
                    .with_translation(Vec3::new(0.0, 0.9, 0.0))
                    .with_scale(Vec3::new(0.85, 0.85, 0.85)),
            },
            SceneInstance {
                id: "north_spire".to_string(),
                mesh: "pyramid".to_string(),
                material: "warm".to_string(),
                transform: Transform::identity()
                    .with_translation(Vec3::new(0.0, 0.25, -2.7))
                    .with_scale(Vec3::new(0.85, 1.65, 0.85)),
            },
            SceneInstance {
                id: "south_spire".to_string(),
                mesh: "pyramid".to_string(),
                material: "warm".to_string(),
                transform: Transform::identity()
                    .with_translation(Vec3::new(0.0, 0.25, 2.7))
                    .with_rotation(Vec3::new(0.0, 1.57, 0.0))
                    .with_scale(Vec3::new(0.85, 1.35, 0.85)),
            },
            SceneInstance {
                id: "east_tower".to_string(),
                mesh: "cube".to_string(),
                material: "glass".to_string(),
                transform: Transform::identity()
                    .with_translation(Vec3::new(2.7, 0.25, 0.0))
                    .with_rotation(Vec3::new(0.08, 0.34, 0.0))
                    .with_scale(Vec3::new(0.55, 1.25, 0.55)),
            },
            SceneInstance {
                id: "west_tower".to_string(),
                mesh: "cube".to_string(),
                material: "glass".to_string(),
                transform: Transform::identity()
                    .with_translation(Vec3::new(-2.7, 0.45, 0.0))
                    .with_rotation(Vec3::new(0.05, -0.22, 0.0))
                    .with_scale(Vec3::new(0.65, 1.55, 0.65)),
            },
        ],
        animations: vec![
            SceneAnimation::Spin {
                instance_id: "orb".to_string(),
                axis_radians_per_second: Vec3::new(0.0, 0.55, 0.0),
            },
            SceneAnimation::Bob {
                instance_id: "orb".to_string(),
                amplitude: 0.20,
                speed_radians_per_second: 1.30,
            },
            SceneAnimation::Spin {
                instance_id: "east_tower".to_string(),
                axis_radians_per_second: Vec3::new(0.0, 0.24, 0.05),
            },
            SceneAnimation::Spin {
                instance_id: "west_tower".to_string(),
                axis_radians_per_second: Vec3::new(0.0, -0.28, 0.04),
            },
        ],
        particle_emitters: vec![ParticleEmitter {
            id: "lantern_dust".to_string(),
            center: Vec3::new(0.0, 1.3, 0.0),
            axis: Vec3::UP,
            radial_range: [1.8, 4.8],
            vertical_range: [-0.4, 1.6],
            particle_size_range: [0.05, 0.12],
            particle_count: 18,
            orbit_radians_per_second: 0.18,
            swirl: 0.22,
            drift: Vec3::new(0.0, 0.22, 0.0),
            color_start: ColorRgb::new(0.38, 0.88, 1.0),
            color_end: ColorRgb::new(1.0, 0.80, 0.48),
            emissive_strength: 0.42,
            softness: 1.2,
            depth_test: false,
        }],
        black_hole: None,
        terrain_surfaces: Vec::new(),
    }
}

fn build_material_atrium_scene() -> SceneDescription {
    let mut scene = AuthoringScene::new("material_atrium");
    scene.viewport_summary =
        "material atrium | primitive-backed renderer showcase | native runtime shell".to_string();
    scene.background = BackgroundGradient {
        top: ColorRgb::new(0.04, 0.08, 0.13),
        bottom: ColorRgb::new(0.10, 0.07, 0.11),
    };
    scene.base_camera = Camera {
        target: Vec3::new(0.0, 0.95, 0.25),
        up: Vec3::UP,
        orbit_radius: 11.2,
        orbit_height: 3.45,
        orbit_speed_radians_per_second: 0.16,
        fov_y_degrees: 46.0,
        near_plane: 0.1,
        far_plane: 160.0,
    };
    scene.base_lighting = LightingRig {
        ambient_color: ColorRgb::new(0.84, 0.85, 0.92),
        ambient_intensity: 0.26,
        directional_lights: vec![
            DirectionalLight {
                direction: Vec3::new(-0.34, -1.0, -0.22).normalize(),
                color: ColorRgb::new(1.0, 0.97, 0.92),
                intensity: 1.22,
            },
            DirectionalLight {
                direction: Vec3::new(0.58, -0.48, 0.36).normalize(),
                color: ColorRgb::new(0.44, 0.66, 0.98),
                intensity: 0.36,
            },
        ],
        point_lights: vec![
            PointLight {
                position: Vec3::new(0.0, 4.2, 0.4),
                color: ColorRgb::new(0.96, 0.92, 0.82),
                intensity: 1.35,
                range: 16.0,
            },
            PointLight {
                position: Vec3::new(-3.2, 1.6, 2.4),
                color: ColorRgb::new(1.0, 0.66, 0.36),
                intensity: 1.08,
                range: 12.0,
            },
            PointLight {
                position: Vec3::new(3.6, 1.8, -2.2),
                color: ColorRgb::new(0.40, 0.86, 1.0),
                intensity: 1.02,
                range: 12.0,
            },
            PointLight {
                position: Vec3::new(0.0, 2.6, 3.6),
                color: ColorRgb::new(0.98, 0.84, 0.54),
                intensity: 0.54,
                range: 10.0,
            },
        ],
    };

    let primitive_library = PrimitiveLibrary::authored_defaults();
    scene.add_primitive_library(&primitive_library);

    scene.add_material(
        "travertine",
        Material {
            base_color: ColorRgb::new(0.75, 0.72, 0.66),
            specular_color: ColorRgb::new(0.94, 0.92, 0.88),
            ambient_strength: 0.24,
            diffuse_strength: 0.88,
            specular_strength: 0.14,
            shininess: 8.0,
        },
    );
    scene.add_material(
        "obsidian",
        Material {
            base_color: ColorRgb::new(0.12, 0.13, 0.17),
            specular_color: ColorRgb::new(0.76, 0.84, 0.96),
            ambient_strength: 0.20,
            diffuse_strength: 0.72,
            specular_strength: 0.58,
            shininess: 44.0,
        },
    );
    scene.add_material(
        "brass",
        Material {
            base_color: ColorRgb::new(0.86, 0.67, 0.29),
            specular_color: ColorRgb::new(1.0, 0.94, 0.70),
            ambient_strength: 0.18,
            diffuse_strength: 0.92,
            specular_strength: 0.44,
            shininess: 26.0,
        },
    );
    scene.add_material(
        "porcelain",
        Material {
            base_color: ColorRgb::new(0.86, 0.90, 0.98),
            specular_color: ColorRgb::new(1.0, 1.0, 1.0),
            ambient_strength: 0.24,
            diffuse_strength: 0.94,
            specular_strength: 0.36,
            shininess: 28.0,
        },
    );
    scene.add_material(
        "glass",
        Material {
            base_color: ColorRgb::new(0.22, 0.70, 0.82),
            specular_color: ColorRgb::new(0.94, 0.99, 1.0),
            ambient_strength: 0.18,
            diffuse_strength: 0.90,
            specular_strength: 0.72,
            shininess: 42.0,
        },
    );
    scene.add_material(
        "apricot",
        Material {
            base_color: ColorRgb::new(0.95, 0.53, 0.34),
            specular_color: ColorRgb::new(1.0, 0.88, 0.76),
            ambient_strength: 0.20,
            diffuse_strength: 0.92,
            specular_strength: 0.30,
            shininess: 18.0,
        },
    );

    let floor = scene.spawn_mesh("atrium_floor", "studio-plane", "travertine");
    set_node_transform(
        &mut scene,
        floor,
        Transform::identity()
            .with_translation(Vec3::new(0.0, -1.0, 0.0))
            .with_scale(Vec3::new(8.8, 1.0, 8.8)),
    );

    let back_wall = scene.spawn_mesh("atrium_back_wall", "studio-plane", "obsidian");
    set_node_transform(
        &mut scene,
        back_wall,
        Transform::identity()
            .with_translation(Vec3::new(0.0, 2.35, -7.25))
            .with_rotation(Vec3::new(-std::f32::consts::FRAC_PI_2, 0.0, 0.0))
            .with_scale(Vec3::new(8.6, 1.0, 4.8)),
    );

    let ceiling = scene.spawn_mesh("atrium_ceiling", "studio-plane", "travertine");
    set_node_transform(
        &mut scene,
        ceiling,
        Transform::identity()
            .with_translation(Vec3::new(0.0, 5.45, 0.0))
            .with_rotation(Vec3::new(std::f32::consts::FRAC_PI_2, 0.0, 0.0))
            .with_scale(Vec3::new(8.6, 1.0, 8.6)),
    );

    let central_orb = scene.spawn_mesh("central_orb", "hero-quad-sphere", "glass");
    set_node_transform(
        &mut scene,
        central_orb,
        Transform::identity()
            .with_translation(Vec3::new(0.0, 1.18, 0.25))
            .with_scale(Vec3::new(1.42, 1.42, 1.42)),
    );

    let halo_ring = scene.spawn_mesh("halo_ring", "hero-torus", "brass");
    set_node_transform(
        &mut scene,
        halo_ring,
        Transform::identity()
            .with_translation(Vec3::new(0.0, 1.72, 0.25))
            .with_rotation(Vec3::new(1.5707964, 0.14, 0.0))
            .with_scale(Vec3::new(2.95, 2.95, 2.95)),
    );

    let brass_monolith = scene.spawn_mesh("brass_monolith", "startup-cube", "brass");
    set_node_transform(
        &mut scene,
        brass_monolith,
        Transform::identity()
            .with_translation(Vec3::new(-2.45, 0.72, -1.55))
            .with_scale(Vec3::new(0.88, 2.45, 0.88)),
    );

    let porcelain_spire = scene.spawn_mesh("porcelain_spire", "hero-cone", "porcelain");
    set_node_transform(
        &mut scene,
        porcelain_spire,
        Transform::identity()
            .with_translation(Vec3::new(2.62, 0.96, 1.78))
            .with_scale(Vec3::new(1.10, 2.28, 1.10)),
    );

    let apricot_lift = scene.spawn_mesh("apricot_lift", "sculpt-capsule", "apricot");
    set_node_transform(
        &mut scene,
        apricot_lift,
        Transform::identity()
            .with_translation(Vec3::new(0.0, 0.28, 3.02))
            .with_scale(Vec3::new(0.88, 1.56, 0.88)),
    );

    let obsidian_plinth = scene.spawn_mesh("obsidian_plinth", "startup-cube", "obsidian");
    set_node_transform(
        &mut scene,
        obsidian_plinth,
        Transform::identity()
            .with_translation(Vec3::new(0.0, -0.12, 3.0))
            .with_scale(Vec3::new(2.10, 0.82, 0.92)),
    );

    let columns = scene.spawn_instancer(
        "atrium_columns",
        Instancer {
            geometry: "studio-cylinder".to_string(),
            material: "obsidian".to_string(),
            pattern: InstancePattern::Points {
                points: vec![
                    Vec3::new(-4.9, 0.0, -4.6),
                    Vec3::new(0.0, 0.0, -5.0),
                    Vec3::new(4.9, 0.0, -4.6),
                    Vec3::new(-5.2, 0.0, 0.0),
                    Vec3::new(5.2, 0.0, 0.0),
                    Vec3::new(-4.9, 0.0, 4.6),
                    Vec3::new(0.0, 0.0, 5.0),
                    Vec3::new(4.9, 0.0, 4.6),
                ],
            },
            effectors: vec![Effector::Scale {
                factor: Vec3::new(0.92, 1.35, 0.92),
                field: Field::Constant(1.0),
            }],
        },
    );
    set_node_transform(
        &mut scene,
        columns,
        Transform::identity().with_translation(Vec3::new(0.0, 0.62, 0.0)),
    );

    let mut description = scene
        .flatten()
        .expect("material atrium authoring should flatten into a renderable scene");
    description.animations = vec![
        SceneAnimation::Spin {
            instance_id: "central_orb".to_string(),
            axis_radians_per_second: Vec3::new(0.0, 0.36, 0.0),
        },
        SceneAnimation::Bob {
            instance_id: "central_orb".to_string(),
            amplitude: 0.12,
            speed_radians_per_second: 1.1,
        },
        SceneAnimation::Spin {
            instance_id: "halo_ring".to_string(),
            axis_radians_per_second: Vec3::new(0.0, 0.15, 0.04),
        },
        SceneAnimation::Spin {
            instance_id: "brass_monolith".to_string(),
            axis_radians_per_second: Vec3::new(0.02, 0.50, 0.0),
        },
        SceneAnimation::Spin {
            instance_id: "porcelain_spire".to_string(),
            axis_radians_per_second: Vec3::new(0.0, -0.28, 0.05),
        },
    ];
    description
}

fn set_node_transform(
    scene: &mut AuthoringScene,
    node_id: NodeId,
    transform: Transform,
) {
    scene
        .node_mut(node_id)
        .expect("material atrium node should exist")
        .transform = transform;
}

fn build_retirement_demo_scene() -> SceneDescription {
    let mut meshes = BTreeMap::new();
    meshes.insert("cube".to_string(), mesh_cube());
    meshes.insert("floor".to_string(), mesh_plane());
    meshes.insert("pyramid".to_string(), mesh_pyramid());

    let mut materials = BTreeMap::new();
    materials.insert(
        "hero".to_string(),
        Material {
            base_color: ColorRgb::new(0.12, 0.63, 0.98),
            specular_color: ColorRgb::new(0.80, 0.95, 1.0),
            ambient_strength: 0.22,
            diffuse_strength: 1.0,
            specular_strength: 0.62,
            shininess: 24.0,
        },
    );
    materials.insert(
        "floor".to_string(),
        Material {
            base_color: ColorRgb::new(0.20, 0.24, 0.29),
            specular_color: ColorRgb::new(0.40, 0.42, 0.48),
            ambient_strength: 0.30,
            diffuse_strength: 0.85,
            specular_strength: 0.16,
            shininess: 8.0,
        },
    );
    materials.insert(
        "accent".to_string(),
        Material {
            base_color: ColorRgb::new(0.95, 0.62, 0.20),
            specular_color: ColorRgb::new(1.0, 0.90, 0.65),
            ambient_strength: 0.20,
            diffuse_strength: 1.0,
            specular_strength: 0.40,
            shininess: 16.0,
        },
    );

    SceneDescription {
        name: "retirement_demo".to_string(),
        viewport_summary: "studio demo | depth | lighting | orbit camera".to_string(),
        background: BackgroundGradient {
            top: ColorRgb::new(0.03, 0.05, 0.08),
            bottom: ColorRgb::new(0.13, 0.17, 0.22),
        },
        camera: Camera {
            target: Vec3::new(0.0, 0.3, 0.0),
            up: Vec3::UP,
            orbit_radius: 6.8,
            orbit_height: 2.9,
            orbit_speed_radians_per_second: 0.35,
            fov_y_degrees: 52.0,
            near_plane: 0.1,
            far_plane: 100.0,
        },
        lighting: LightingRig {
            ambient_color: ColorRgb::new(0.75, 0.80, 0.95),
            ambient_intensity: 0.32,
            directional_lights: vec![
                DirectionalLight {
                    direction: Vec3::new(-0.55, -1.0, -0.40).normalize(),
                    color: ColorRgb::new(0.95, 0.96, 1.0),
                    intensity: 1.25,
                },
                DirectionalLight {
                    direction: Vec3::new(0.55, -0.6, 0.30).normalize(),
                    color: ColorRgb::new(0.25, 0.40, 0.80),
                    intensity: 0.28,
                },
            ],
            point_lights: vec![
                PointLight {
                    position: Vec3::new(1.8, 2.2, 1.4),
                    color: ColorRgb::new(1.0, 0.75, 0.50),
                    intensity: 1.2,
                    range: 8.0,
                },
                PointLight {
                    position: Vec3::new(-2.2, 1.5, -1.4),
                    color: ColorRgb::new(0.35, 0.80, 1.0),
                    intensity: 0.85,
                    range: 7.0,
                },
            ],
        },
        meshes,
        materials,
        instances: vec![
            SceneInstance {
                id: "hero_cube".to_string(),
                mesh: "cube".to_string(),
                material: "hero".to_string(),
                transform: Transform::identity()
                    .with_translation(Vec3::new(0.0, 0.35, 0.0))
                    .with_scale(Vec3::new(1.35, 1.35, 1.35)),
            },
            SceneInstance {
                id: "accent_cube".to_string(),
                mesh: "cube".to_string(),
                material: "accent".to_string(),
                transform: Transform::identity()
                    .with_translation(Vec3::new(-2.2, 0.55, 1.15))
                    .with_rotation(Vec3::new(0.1, 0.3, 0.0))
                    .with_scale(Vec3::new(0.65, 0.65, 0.65)),
            },
            SceneInstance {
                id: "spire".to_string(),
                mesh: "pyramid".to_string(),
                material: "accent".to_string(),
                transform: Transform::identity()
                    .with_translation(Vec3::new(2.15, 0.35, -1.25))
                    .with_scale(Vec3::new(0.95, 1.40, 0.95)),
            },
            SceneInstance {
                id: "floor".to_string(),
                mesh: "floor".to_string(),
                material: "floor".to_string(),
                transform: Transform::identity()
                    .with_translation(Vec3::new(0.0, -1.0, 0.0))
                    .with_scale(Vec3::new(5.5, 1.0, 5.5)),
            },
        ],
        animations: vec![
            SceneAnimation::Spin {
                instance_id: "hero_cube".to_string(),
                axis_radians_per_second: Vec3::new(0.25, 0.90, 0.10),
            },
            SceneAnimation::Spin {
                instance_id: "accent_cube".to_string(),
                axis_radians_per_second: Vec3::new(0.0, -1.35, 0.15),
            },
            SceneAnimation::Bob {
                instance_id: "accent_cube".to_string(),
                amplitude: 0.18,
                speed_radians_per_second: 1.9,
            },
            SceneAnimation::Spin {
                instance_id: "spire".to_string(),
                axis_radians_per_second: Vec3::new(0.0, 0.50, 0.0),
            },
        ],
        particle_emitters: Vec::new(),
        black_hole: None,
        terrain_surfaces: Vec::new(),
    }
}

fn build_kerr_black_hole_scene() -> SceneDescription {
    let spin_axis = Vec3::new(0.35, 1.0, 0.18).normalize();
    SceneDescription {
        name: "kerr_black_hole".to_string(),
        viewport_summary: "kerr singularity | accretion particles | lens shell".to_string(),
        background: BackgroundGradient {
            top: ColorRgb::new(0.01, 0.02, 0.05),
            bottom: ColorRgb::new(0.0, 0.0, 0.01),
        },
        camera: Camera {
            target: Vec3::new(0.0, 0.2, 0.0),
            up: Vec3::UP,
            orbit_radius: 10.0,
            orbit_height: 2.8,
            orbit_speed_radians_per_second: 0.18,
            fov_y_degrees: 58.0,
            near_plane: 0.05,
            far_plane: 220.0,
        },
        lighting: LightingRig {
            ambient_color: ColorRgb::new(0.55, 0.58, 0.82),
            ambient_intensity: 0.12,
            directional_lights: vec![
                DirectionalLight {
                    direction: Vec3::new(-0.15, -1.0, -0.35).normalize(),
                    color: ColorRgb::new(0.28, 0.42, 0.95),
                    intensity: 0.42,
                },
                DirectionalLight {
                    direction: Vec3::new(0.45, -0.25, 0.05).normalize(),
                    color: ColorRgb::new(0.98, 0.62, 0.20),
                    intensity: 0.26,
                },
            ],
            point_lights: vec![
                PointLight {
                    position: Vec3::new(2.5, 0.7, 0.0),
                    color: ColorRgb::new(1.0, 0.62, 0.22),
                    intensity: 1.85,
                    range: 10.0,
                },
                PointLight {
                    position: Vec3::new(-2.0, 1.3, -1.4),
                    color: ColorRgb::new(0.48, 0.72, 1.0),
                    intensity: 1.15,
                    range: 12.0,
                },
            ],
        },
        meshes: BTreeMap::new(),
        materials: BTreeMap::new(),
        instances: Vec::new(),
        animations: Vec::new(),
        particle_emitters: vec![
            ParticleEmitter {
                id: "accretion_inner".to_string(),
                center: Vec3::new(0.0, 0.0, 0.0),
                axis: spin_axis,
                radial_range: [1.55, 2.75],
                vertical_range: [-0.18, 0.18],
                particle_size_range: [0.10, 0.28],
                particle_count: 60,
                orbit_radians_per_second: 1.90,
                swirl: 0.55,
                drift: Vec3::new(0.0, 0.16, 0.0),
                color_start: ColorRgb::new(1.0, 0.62, 0.18),
                color_end: ColorRgb::new(1.0, 0.92, 0.58),
                emissive_strength: 0.78,
                softness: 1.8,
                depth_test: true,
            },
            ParticleEmitter {
                id: "accretion_outer".to_string(),
                center: Vec3::new(0.0, 0.0, 0.0),
                axis: spin_axis,
                radial_range: [2.6, 4.8],
                vertical_range: [-0.34, 0.34],
                particle_size_range: [0.08, 0.22],
                particle_count: 72,
                orbit_radians_per_second: 1.05,
                swirl: 0.35,
                drift: Vec3::new(0.0, 0.22, 0.0),
                color_start: ColorRgb::new(0.34, 0.56, 1.0),
                color_end: ColorRgb::new(1.0, 0.52, 0.15),
                emissive_strength: 0.62,
                softness: 2.0,
                depth_test: true,
            },
            ParticleEmitter {
                id: "north_jet".to_string(),
                center: spin_axis * 0.9,
                axis: spin_axis,
                radial_range: [0.08, 0.46],
                vertical_range: [-0.05, 0.05],
                particle_size_range: [0.06, 0.18],
                particle_count: 24,
                orbit_radians_per_second: 3.5,
                swirl: 0.90,
                drift: spin_axis * 11.5,
                color_start: ColorRgb::new(0.72, 0.88, 1.0),
                color_end: ColorRgb::new(0.34, 0.72, 1.0),
                emissive_strength: 0.85,
                softness: 1.4,
                depth_test: false,
            },
            ParticleEmitter {
                id: "south_jet".to_string(),
                center: spin_axis * -0.9,
                axis: spin_axis,
                radial_range: [0.08, 0.42],
                vertical_range: [-0.05, 0.05],
                particle_size_range: [0.06, 0.18],
                particle_count: 24,
                orbit_radians_per_second: -3.5,
                swirl: 0.90,
                drift: spin_axis * -11.5,
                color_start: ColorRgb::new(0.55, 0.78, 1.0),
                color_end: ColorRgb::new(0.92, 0.98, 1.0),
                emissive_strength: 0.80,
                softness: 1.35,
                depth_test: false,
            },
            ParticleEmitter {
                id: "star_halo".to_string(),
                center: Vec3::new(0.0, 0.0, 0.0),
                axis: Vec3::UP,
                radial_range: [8.0, 18.0],
                vertical_range: [-7.5, 7.5],
                particle_size_range: [0.03, 0.10],
                particle_count: 64,
                orbit_radians_per_second: 0.08,
                swirl: 0.05,
                drift: Vec3::ZERO,
                color_start: ColorRgb::new(0.55, 0.70, 1.0),
                color_end: ColorRgb::new(1.0, 1.0, 1.0),
                emissive_strength: 0.28,
                softness: 1.1,
                depth_test: false,
            },
        ],
        black_hole: Some(BlackHole {
            center: Vec3::ZERO,
            radius: 1.28,
            lens_radius: 2.15,
            spin_axis,
            inner_color: ColorRgb::new(0.01, 0.01, 0.02),
            lens_color: ColorRgb::new(0.25, 0.55, 1.0),
            disk_color: ColorRgb::new(1.0, 0.64, 0.18),
        }),
        terrain_surfaces: Vec::new(),
    }
}

fn sample_terrain_height(
    surface: &TerrainSurface,
    local_x: f32,
    local_z: f32,
    time_seconds: f32,
) -> f32 {
    let radius = (local_x * local_x + local_z * local_z).sqrt();
    let basin_t = 1.0
        - smoothstep(
            surface.caldera_radius * 0.20,
            surface.caldera_radius.max(surface.caldera_radius * 0.95),
            radius.min(1.0),
        );
    let rim_t = smoothstep(surface.caldera_radius * 0.72, 1.0, radius.min(1.0));
    let ridge_noise = ((local_x * 4.4 + time_seconds * 0.22).sin()
        * (local_z * 3.8 - time_seconds * 0.16).cos())
        * surface.height_amplitude
        * 0.14;
    let flow_wave = (((local_x + local_z) * surface.ripple_frequency)
        + time_seconds * surface.flow_speed)
        .sin()
        * surface.ripple_amplitude;
    let counter_wave = (((local_x - local_z) * (surface.ripple_frequency * 0.72))
        - time_seconds * (surface.flow_speed * 0.58))
        .cos()
        * surface.ripple_amplitude
        * 0.55;
    let core_heat = (1.0 - smoothstep(0.0, surface.caldera_radius * 0.88, radius.min(1.0)))
        * surface.height_amplitude
        * 0.34
        * ((time_seconds * (surface.flow_speed * 1.85)) + radius * 11.0).sin();
    let raw_height = surface.base_height + rim_t * surface.rim_strength
        - basin_t * surface.caldera_depth
        + ridge_noise
        + flow_wave
        + counter_wave
        + core_heat;
    if surface.terrace_step <= f32::EPSILON {
        raw_height
    } else {
        let terraced = (raw_height / surface.terrace_step).round() * surface.terrace_step;
        terraced + flow_wave * 0.22 + core_heat * 0.35
    }
}

fn smoothstep(edge0: f32, edge1: f32, value: f32) -> f32 {
    if (edge1 - edge0).abs() <= f32::EPSILON {
        return if value >= edge1 { 1.0 } else { 0.0 };
    }
    let t = ((value - edge0) / (edge1 - edge0)).clamp(0.0, 1.0);
    t * t * (3.0 - 2.0 * t)
}

fn mesh_cube() -> Mesh {
    primitive_mesh(PrimitiveShape::Box {
        size: Vec3::new(2.0, 2.0, 2.0),
        width_segments: 1,
        height_segments: 1,
        depth_segments: 1,
    })
}

fn mesh_plane() -> Mesh {
    primitive_mesh(PrimitiveShape::Plane {
        size: crate::Vec2::new(2.0, 2.0),
        width_segments: 1,
        depth_segments: 1,
    })
}

fn mesh_pyramid() -> Mesh {
    let apex = Vec3::new(0.0, 1.0, 0.0);
    let base = [
        Vec3::new(-1.0, -1.0, 1.0),
        Vec3::new(1.0, -1.0, 1.0),
        Vec3::new(1.0, -1.0, -1.0),
        Vec3::new(-1.0, -1.0, -1.0),
    ];

    let mut vertices = Vec::new();
    let mut triangles = Vec::new();

    for face in 0..4 {
        let next = (face + 1) % 4;
        let p0 = base[face];
        let p1 = base[next];
        let normal = (p1 - p0).cross(apex - p0).normalize();
        let base_index = vertices.len();
        vertices.push(Vertex {
            position: p0,
            normal,
        });
        vertices.push(Vertex {
            position: p1,
            normal,
        });
        vertices.push(Vertex {
            position: apex,
            normal,
        });
        triangles.push([base_index, base_index + 1, base_index + 2]);
    }

    let base_index = vertices.len();
    vertices.extend(base.map(|position| Vertex {
        position,
        normal: Vec3::new(0.0, -1.0, 0.0),
    }));
    triangles.push([base_index, base_index + 2, base_index + 1]);
    triangles.push([base_index, base_index + 3, base_index + 2]);

    Mesh {
        vertices,
        triangles,
    }
}

fn mesh_uv_sphere(latitude_segments: usize, longitude_segments: usize) -> Mesh {
    primitive_mesh(PrimitiveShape::UvSphere {
        radius: 1.0,
        latitude_segments,
        longitude_segments,
    })
}

fn primitive_mesh(shape: PrimitiveShape) -> Mesh {
    shape
        .build_mesh()
        .expect("scene primitive should always convert into a render mesh")
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn default_catalog_contains_black_hole_scene() {
        let catalog = SceneCatalog::default();
        let dcc_suite_scene = catalog
            .scene("dcc_suite_scene")
            .expect("dcc suite startup scene should be registered");
        let tensor_stream_scene = catalog
            .scene("tensor_stream_probe")
            .expect("tensor stream probe scene should be registered");
        let default_scene = catalog
            .scene("luminous_port")
            .expect("luminous port scene should be registered");
        let magma_scene = catalog
            .scene("magma_terraces")
            .expect("magma terraces scene should be registered");
        let material_atrium_scene = catalog
            .scene("material_atrium")
            .expect("material atrium scene should be registered");
        let scene = catalog
            .scene("kerr_black_hole")
            .expect("black hole scene should be registered");

        let compute_alias_scene = catalog
            .scene("gpu_compute_surface_probe")
            .expect("compute smoke alias should resolve");
        let starforge_alias_scene = catalog
            .scene("starforge")
            .expect("starforge alias should resolve");
        let atrium_alias_scene = catalog
            .scene("renderer_atrium")
            .expect("renderer atrium alias should resolve");
        let dcc_alias_scene = catalog
            .scene("dcc_authoring_startup")
            .expect("dcc startup alias should resolve");

        assert_eq!(catalog.default_scene, "luminous_port");
        assert_eq!(dcc_suite_scene.name, "dcc_suite_scene");
        assert_eq!(dcc_suite_scene.instances.len(), 3);
        assert!(dcc_suite_scene
            .instances
            .iter()
            .any(|instance| instance.id == "blender_startup_cube"));
        assert_eq!(tensor_stream_scene.name, "tensor_stream_probe");
        assert!(tensor_stream_scene.particle_emitters.len() >= 2);
        assert_eq!(default_scene.name, "luminous_port");
        assert!(default_scene.black_hole.is_none());
        assert_eq!(compute_alias_scene.name, "tensor_stream_probe");
        assert_eq!(starforge_alias_scene.name, "luminous_port");
        assert_eq!(atrium_alias_scene.name, "material_atrium");
        assert_eq!(dcc_alias_scene.name, "dcc_suite_scene");
        assert_eq!(magma_scene.name, "magma_terraces");
        assert_eq!(material_atrium_scene.name, "material_atrium");
        assert!(material_atrium_scene.instances.len() >= 8);
        assert!(material_atrium_scene.lighting.point_lights.len() >= 3);
        assert!(magma_scene.instances.len() >= 150);
        assert!(magma_scene.particle_emitters.len() >= 8);
        assert!(magma_scene.lighting.point_lights.len() >= 8);
        assert!(!magma_scene.terrain_surfaces.is_empty());
        assert!(magma_scene
            .resolved_mesh("terrain_heightfield", 1.0)
            .is_some());
        assert!(magma_scene
            .ground_height_at(Vec3::new(0.0, 2.0, 0.0), 1.0)
            .is_some());
        assert_eq!(scene.name, "kerr_black_hole");
        assert!(!scene.particle_emitters.is_empty());
        assert!(scene.black_hole.is_some());
    }

    #[test]
    fn animated_instances_can_be_overridden_per_view() {
        let catalog = SceneCatalog::default();
        let magma_scene = catalog
            .scene("magma_terraces")
            .expect("magma terraces scene should be registered");
        let mut overrides = BTreeMap::new();
        overrides.insert(
            "terrain_body".to_string(),
            Transform::identity().with_translation(Vec3::new(3.0, 4.0, 5.0)),
        );

        let instances = magma_scene.animated_instances_with_overrides(0.5, &overrides);
        let terrain_body = instances
            .into_iter()
            .find(|instance| instance.id == "terrain_body")
            .expect("terrain body should still exist");
        assert_eq!(terrain_body.transform.translation, Vec3::new(3.0, 4.0, 5.0));
    }
}
