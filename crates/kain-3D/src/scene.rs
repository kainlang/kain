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

impl Camera {
    pub fn position_at(&self, time_seconds: f32) -> Vec3 {
        let angle = time_seconds * self.orbit_speed_radians_per_second;
        Vec3::new(
            self.target.x + angle.cos() * self.orbit_radius,
            self.target.y + self.orbit_height,
            self.target.z + angle.sin() * self.orbit_radius,
        )
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
pub struct SceneDescription {
    pub name: String,
    pub background: BackgroundGradient,
    pub camera: Camera,
    pub lighting: LightingRig,
    pub meshes: BTreeMap<String, Mesh>,
    pub materials: BTreeMap<String, Material>,
    pub instances: Vec<SceneInstance>,
    pub animations: Vec<SceneAnimation>,
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
        let retirement_demo = build_retirement_demo_scene();
        let mut scenes = BTreeMap::new();
        scenes.insert(retirement_demo.name.clone(), retirement_demo);

        Self {
            default_scene: "retirement_demo".to_string(),
            scenes,
        }
    }
}

impl SceneCatalog {
    pub fn scene(&self, name: &str) -> Option<&SceneDescription> {
        self.scenes.get(name).or_else(|| self.scenes.get(&self.default_scene))
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
