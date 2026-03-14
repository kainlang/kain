use std::collections::BTreeMap;

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
                    if let Some(instance) =
                        instances.iter_mut().find(|candidate| candidate.id == *instance_id)
                    {
                        instance.transform.rotation_radians += *axis_radians_per_second * time_seconds;
                    }
                }
                SceneAnimation::Bob {
                    instance_id,
                    amplitude,
                    speed_radians_per_second,
                } => {
                    if let Some(instance) =
                        instances.iter_mut().find(|candidate| candidate.id == *instance_id)
                    {
                        instance.transform.translation.y +=
                            amplitude * (time_seconds * speed_radians_per_second).sin();
                    }
                }
            }
        }
        instances
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct SceneCatalog {
    pub default_scene: String,
    pub scenes: BTreeMap<String, SceneDescription>,
}

impl Default for SceneCatalog {
    fn default() -> Self {
        let luminous_port = build_luminous_port_scene();
        let retirement_demo = build_retirement_demo_scene();
        let kerr_black_hole = build_kerr_black_hole_scene();
        let mut scenes = BTreeMap::new();
        scenes.insert(luminous_port.name.clone(), luminous_port);
        scenes.insert(retirement_demo.name.clone(), retirement_demo);
        scenes.insert(kerr_black_hole.name.clone(), kerr_black_hole);

        Self {
            default_scene: "luminous_port".to_string(),
            scenes,
        }
    }
}

impl SceneCatalog {
    pub fn scene(&self, name: &str) -> Option<&SceneDescription> {
        self.scenes
            .get(name)
            .or_else(|| self.scenes.get(&self.default_scene))
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
    }
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
    }
}

fn mesh_cube() -> Mesh {
    let positions = [
        Vec3::new(-1.0, -1.0, 1.0),
        Vec3::new(1.0, -1.0, 1.0),
        Vec3::new(1.0, 1.0, 1.0),
        Vec3::new(-1.0, 1.0, 1.0),
        Vec3::new(-1.0, -1.0, -1.0),
        Vec3::new(1.0, -1.0, -1.0),
        Vec3::new(1.0, 1.0, -1.0),
        Vec3::new(-1.0, 1.0, -1.0),
    ];
    let faces = [
        ([0, 1, 2, 3], Vec3::new(0.0, 0.0, 1.0)),
        ([5, 4, 7, 6], Vec3::new(0.0, 0.0, -1.0)),
        ([4, 0, 3, 7], Vec3::new(-1.0, 0.0, 0.0)),
        ([1, 5, 6, 2], Vec3::new(1.0, 0.0, 0.0)),
        ([3, 2, 6, 7], Vec3::new(0.0, 1.0, 0.0)),
        ([4, 5, 1, 0], Vec3::new(0.0, -1.0, 0.0)),
    ];

    let mut vertices = Vec::new();
    let mut triangles = Vec::new();
    for (indices, normal) in faces {
        let base = vertices.len();
        vertices.extend(indices.map(|index| Vertex {
            position: positions[index],
            normal,
        }));
        triangles.push([base, base + 1, base + 2]);
        triangles.push([base, base + 2, base + 3]);
    }

    Mesh { vertices, triangles }
}

fn mesh_plane() -> Mesh {
    Mesh {
        vertices: vec![
            Vertex {
                position: Vec3::new(-1.0, 0.0, -1.0),
                normal: Vec3::UP,
            },
            Vertex {
                position: Vec3::new(1.0, 0.0, -1.0),
                normal: Vec3::UP,
            },
            Vertex {
                position: Vec3::new(1.0, 0.0, 1.0),
                normal: Vec3::UP,
            },
            Vertex {
                position: Vec3::new(-1.0, 0.0, 1.0),
                normal: Vec3::UP,
            },
        ],
        triangles: vec![[0, 1, 2], [0, 2, 3]],
    }
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
        vertices.push(Vertex { position: p0, normal });
        vertices.push(Vertex { position: p1, normal });
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

    Mesh { vertices, triangles }
}

fn mesh_uv_sphere(latitude_segments: usize, longitude_segments: usize) -> Mesh {
    let latitude_segments = latitude_segments.max(3);
    let longitude_segments = longitude_segments.max(4);
    let mut vertices = Vec::new();
    let mut triangles = Vec::new();

    for latitude in 0..=latitude_segments {
        let v = latitude as f32 / latitude_segments as f32;
        let phi = v * std::f32::consts::PI;
        let y = phi.cos();
        let ring_radius = phi.sin();
        for longitude in 0..=longitude_segments {
            let u = longitude as f32 / longitude_segments as f32;
            let theta = u * std::f32::consts::TAU;
            let normal = Vec3::new(theta.cos() * ring_radius, y, theta.sin() * ring_radius);
            vertices.push(Vertex {
                position: normal,
                normal: normal.normalize(),
            });
        }
    }

    let stride = longitude_segments + 1;
    for latitude in 0..latitude_segments {
        for longitude in 0..longitude_segments {
            let current = latitude * stride + longitude;
            let next = current + stride;
            triangles.push([current, next, current + 1]);
            triangles.push([current + 1, next, next + 1]);
        }
    }

    Mesh { vertices, triangles }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn default_catalog_contains_black_hole_scene() {
        let catalog = SceneCatalog::default();
        let default_scene = catalog
            .scene("luminous_port")
            .expect("luminous port scene should be registered");
        let scene = catalog
            .scene("kerr_black_hole")
            .expect("black hole scene should be registered");

        assert_eq!(catalog.default_scene, "luminous_port");
        assert_eq!(default_scene.name, "luminous_port");
        assert!(default_scene.black_hole.is_none());
        assert_eq!(scene.name, "kerr_black_hole");
        assert!(!scene.particle_emitters.is_empty());
        assert!(scene.black_hole.is_some());
    }
}
