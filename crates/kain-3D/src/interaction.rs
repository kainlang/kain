use crate::{CameraPose, RenderResolution, SceneCatalog, SceneDescription, Vec3};

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct PickTargetId {
    pub instance_id: String,
    pub mesh_id: String,
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct PickingRay {
    pub origin: Vec3,
    pub direction: Vec3,
}

impl PickingRay {
    pub fn new(origin: Vec3, direction: Vec3) -> Self {
        Self {
            origin,
            direction: direction.normalize(),
        }
    }

    pub fn from_viewport_pixel(
        pixel_x: f32,
        pixel_y: f32,
        resolution: RenderResolution,
        camera: &CameraPose,
    ) -> Self {
        let width = resolution.width.max(1) as f32;
        let height = resolution.height.max(1) as f32;
        let ndc_x = ((pixel_x / width) * 2.0) - 1.0;
        let ndc_y = 1.0 - ((pixel_y / height) * 2.0);
        let aspect_ratio = width / height;
        let tan_half_fov = (camera.fov_y_degrees.to_radians() * 0.5).tan();

        let forward = camera.forward();
        let right = forward.cross(camera.up).normalize();
        let up = right.cross(forward).normalize();
        let direction =
            (forward + right * (ndc_x * aspect_ratio * tan_half_fov) + up * (ndc_y * tan_half_fov))
                .normalize();

        Self::new(camera.position, direction)
    }
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct PickingQuery {
    pub ray: PickingRay,
    pub scene_time_seconds: f32,
    pub max_distance: f32,
}

impl PickingQuery {
    pub fn new(ray: PickingRay, scene_time_seconds: f32) -> Self {
        Self {
            ray,
            scene_time_seconds,
            max_distance: f32::INFINITY,
        }
    }

    pub fn with_max_distance(mut self, max_distance: f32) -> Self {
        self.max_distance = max_distance.max(0.0);
        self
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct PickingHit {
    pub target: PickTargetId,
    pub distance: f32,
    pub position: Vec3,
    pub normal: Vec3,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ManipulatorMode {
    Translate,
    Rotate,
    Scale,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ManipulatorSpace {
    World,
    Local,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ManipulatorAxis {
    X,
    Y,
    Z,
    PlaneXY,
    PlaneXZ,
    PlaneYZ,
    Screen,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ManipulatorState {
    Idle,
    Hover(ManipulatorAxis),
    Active(ManipulatorAxis),
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct ManipulatorDelta {
    pub translation: Vec3,
    pub rotation_radians: Vec3,
    pub scale: Vec3,
}

impl Default for ManipulatorDelta {
    fn default() -> Self {
        Self {
            translation: Vec3::ZERO,
            rotation_radians: Vec3::ZERO,
            scale: Vec3::new(1.0, 1.0, 1.0),
        }
    }
}

#[derive(Clone, Debug, PartialEq)]
pub enum SceneCommand {
    Select {
        target: Option<PickTargetId>,
    },
    SetTransform {
        target: PickTargetId,
        translation: Option<Vec3>,
        rotation_radians: Option<Vec3>,
        scale: Option<Vec3>,
    },
    SetVisibility {
        target: PickTargetId,
        visible: bool,
    },
}

#[derive(Clone, Debug, PartialEq)]
pub struct SceneTransaction {
    pub label: String,
    pub commands: Vec<SceneCommand>,
}

pub trait PickingService {
    fn pick_scene(&self, scene: &SceneDescription, query: &PickingQuery) -> Option<PickingHit>;

    fn pick_catalog_scene(
        &self,
        catalog: &SceneCatalog,
        scene_name: &str,
        query: &PickingQuery,
    ) -> Option<PickingHit> {
        let scene = catalog.scene(scene_name)?;
        self.pick_scene(scene, query)
    }
}

#[derive(Clone, Copy, Debug, Default)]
pub struct CpuPickingService;

impl PickingService for CpuPickingService {
    fn pick_scene(&self, scene: &SceneDescription, query: &PickingQuery) -> Option<PickingHit> {
        self.pick_scene_filtered(scene, query, None)
    }
}

impl CpuPickingService {
    pub fn pick_scene_instance(
        &self,
        scene: &SceneDescription,
        query: &PickingQuery,
        instance_id: &str,
    ) -> Option<PickingHit> {
        self.pick_scene_filtered(scene, query, Some(instance_id))
    }

    pub fn pick_catalog_scene_instance(
        &self,
        catalog: &SceneCatalog,
        scene_name: &str,
        query: &PickingQuery,
        instance_id: &str,
    ) -> Option<PickingHit> {
        let scene = catalog.scene(scene_name)?;
        self.pick_scene_instance(scene, query, instance_id)
    }

    fn pick_scene_filtered(
        &self,
        scene: &SceneDescription,
        query: &PickingQuery,
        instance_filter: Option<&str>,
    ) -> Option<PickingHit> {
        let mut closest_hit: Option<PickingHit> = None;

        for instance in scene.animated_instances(query.scene_time_seconds) {
            if let Some(instance_filter) = instance_filter {
                if instance.id != instance_filter {
                    continue;
                }
            }
            let mesh = scene.resolved_mesh(&instance.mesh, query.scene_time_seconds)?;
            let mesh = mesh.as_ref();
            let model = instance.transform.matrix();

            for triangle in &mesh.triangles {
                let [ia, ib, ic] = *triangle;
                let a = to_vec3(model.transform_point(mesh.vertices[ia].position));
                let b = to_vec3(model.transform_point(mesh.vertices[ib].position));
                let c = to_vec3(model.transform_point(mesh.vertices[ic].position));

                if let Some((distance, position, normal)) =
                    intersect_ray_triangle(query.ray, a, b, c, query.max_distance)
                {
                    let replace = closest_hit
                        .as_ref()
                        .is_none_or(|current| distance < current.distance);
                    if replace {
                        closest_hit = Some(PickingHit {
                            target: PickTargetId {
                                instance_id: instance.id.clone(),
                                mesh_id: instance.mesh.clone(),
                            },
                            distance,
                            position,
                            normal,
                        });
                    }
                }
            }
        }

        closest_hit
    }
}

fn intersect_ray_triangle(
    ray: PickingRay,
    a: Vec3,
    b: Vec3,
    c: Vec3,
    max_distance: f32,
) -> Option<(f32, Vec3, Vec3)> {
    let edge_ab = b - a;
    let edge_ac = c - a;
    let pvec = ray.direction.cross(edge_ac);
    let determinant = edge_ab.dot(pvec);
    if determinant.abs() <= 1.0e-6 {
        return None;
    }

    let inv_det = 1.0 / determinant;
    let tvec = ray.origin - a;
    let u = tvec.dot(pvec) * inv_det;
    if !(0.0..=1.0).contains(&u) {
        return None;
    }

    let qvec = tvec.cross(edge_ab);
    let v = ray.direction.dot(qvec) * inv_det;
    if v < 0.0 || u + v > 1.0 {
        return None;
    }

    let distance = edge_ac.dot(qvec) * inv_det;
    if distance <= 1.0e-4 || distance > max_distance {
        return None;
    }

    let position = ray.origin + ray.direction * distance;
    let normal = edge_ab.cross(edge_ac).normalize();
    Some((distance, position, normal))
}

fn to_vec3(position: [f32; 4]) -> Vec3 {
    let w = if position[3].abs() <= f32::EPSILON {
        1.0
    } else {
        position[3]
    };
    Vec3::new(position[0] / w, position[1] / w, position[2] / w)
}

#[cfg(test)]
mod tests {
    use std::collections::BTreeMap;

    use crate::{
        BackgroundGradient, Camera, ColorRgb, DirectionalLight, Geometry, LightingRig, Material,
        PointLight, SceneDescription, SceneInstance, Transform, Vec3,
    };

    use super::*;

    fn single_cube_scene() -> SceneDescription {
        let mesh = Geometry::box_mesh(Vec3::new(2.0, 2.0, 2.0))
            .to_mesh()
            .expect("box mesh should build");
        let mut meshes = BTreeMap::new();
        meshes.insert("cube".to_string(), mesh);

        let mut materials = BTreeMap::new();
        materials.insert(
            "matte".to_string(),
            Material {
                base_color: ColorRgb::new(0.2, 0.6, 0.9),
                specular_color: ColorRgb::new(1.0, 1.0, 1.0),
                ambient_strength: 0.2,
                diffuse_strength: 0.8,
                specular_strength: 0.1,
                shininess: 8.0,
            },
        );

        SceneDescription {
            name: "pick_test".to_string(),
            viewport_summary: "pick test".to_string(),
            background: BackgroundGradient {
                top: ColorRgb::BLACK,
                bottom: ColorRgb::BLACK,
            },
            camera: Camera {
                target: Vec3::ZERO,
                up: Vec3::UP,
                orbit_radius: 6.0,
                orbit_height: 0.0,
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
                point_lights: vec![PointLight {
                    position: Vec3::new(0.0, 3.0, 3.0),
                    color: ColorRgb::WHITE,
                    intensity: 0.8,
                    range: 10.0,
                }],
            },
            meshes,
            materials,
            instances: vec![SceneInstance {
                id: "hero".to_string(),
                mesh: "cube".to_string(),
                material: "matte".to_string(),
                transform: Transform::identity(),
            }],
            animations: vec![],
            particle_emitters: vec![],
            black_hole: None,
            terrain_surfaces: vec![],
        }
    }

    #[test]
    fn center_ray_hits_cube() {
        let scene = single_cube_scene();
        let camera = CameraPose {
            position: Vec3::new(0.0, 0.0, 6.0),
            target: Vec3::ZERO,
            up: Vec3::UP,
            fov_y_degrees: 60.0,
            near_plane: 0.1,
            far_plane: 100.0,
        };
        let resolution = RenderResolution::new(400, 300);
        let ray = PickingRay::from_viewport_pixel(200.0, 150.0, resolution, &camera);
        let hit = CpuPickingService
            .pick_scene(&scene, &PickingQuery::new(ray, 0.0))
            .expect("center ray should hit the cube");

        assert_eq!(hit.target.instance_id, "hero");
        assert!(hit.distance > 0.0);
        assert!(hit.position.z <= 1.1);
    }

    #[test]
    fn offscreen_ray_misses_cube() {
        let scene = single_cube_scene();
        let camera = CameraPose {
            position: Vec3::new(0.0, 0.0, 6.0),
            target: Vec3::ZERO,
            up: Vec3::UP,
            fov_y_degrees: 60.0,
            near_plane: 0.1,
            far_plane: 100.0,
        };
        let resolution = RenderResolution::new(400, 300);
        let ray = PickingRay::from_viewport_pixel(399.0, 0.0, resolution, &camera);
        let hit = CpuPickingService.pick_scene(&scene, &PickingQuery::new(ray, 0.0));

        assert!(hit.is_none());
    }
}
