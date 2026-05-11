use std::collections::BTreeMap;

use crate::{
    CameraPose, Mat4, RenderResolution, SceneCatalog, SceneDescription, Transform, Vec2, Vec3,
};

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

#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct ManipulatorSnapSettings {
    pub translation_step: Option<f32>,
    pub rotation_step_radians: Option<f32>,
    pub scale_step: Option<f32>,
}

pub fn apply_manipulator_drag(
    camera: &CameraPose,
    resolution: RenderResolution,
    drag_origin_transform: &Transform,
    pointer_delta: Vec2,
    manipulator_mode: ManipulatorMode,
    manipulator_space: ManipulatorSpace,
    manipulator_axis: ManipulatorAxis,
    snap_enabled: bool,
    snap_settings: ManipulatorSnapSettings,
) -> Transform {
    let viewport_scale = (resolution.width.min(resolution.height) as f32).max(1.0);
    let depth_scale = (camera.position.distance(drag_origin_transform.translation) * 2.4
        / viewport_scale)
        .clamp(0.006, 0.18);
    let camera_right = camera.right();
    let camera_up = camera.right().cross(camera.forward()).normalize();

    let mut updated_transform = drag_origin_transform.clone();
    match manipulator_mode {
        ManipulatorMode::Translate => {
            let translation_delta = match manipulator_axis {
                ManipulatorAxis::Screen => {
                    camera_right * (pointer_delta.x * depth_scale)
                        + camera_up * (-pointer_delta.y * depth_scale)
                }
                ManipulatorAxis::X | ManipulatorAxis::Y | ManipulatorAxis::Z => {
                    let axis = manipulator_direction(
                        drag_origin_transform,
                        manipulator_space,
                        manipulator_axis,
                    );
                    axis * projected_drag_amount(pointer_delta, axis, camera_right, camera_up)
                        * depth_scale
                }
                ManipulatorAxis::PlaneXY | ManipulatorAxis::PlaneXZ | ManipulatorAxis::PlaneYZ => {
                    let (basis_u, basis_v) = manipulator_plane_basis(
                        drag_origin_transform,
                        manipulator_space,
                        manipulator_axis,
                    );
                    basis_u * (pointer_delta.x * depth_scale)
                        + basis_v * (-pointer_delta.y * depth_scale)
                }
            };
            updated_transform.translation = drag_origin_transform.translation + translation_delta;
            if snap_enabled {
                if let Some(step) = snap_settings.translation_step {
                    updated_transform.translation = snap_vec3(updated_transform.translation, step);
                }
            }
        }
        ManipulatorMode::Rotate => {
            match manipulator_axis {
                ManipulatorAxis::Screen => {
                    updated_transform.rotation_radians = drag_origin_transform.rotation_radians
                        + Vec3::new(pointer_delta.y * -0.008, pointer_delta.x * 0.008, 0.0);
                }
                ManipulatorAxis::X | ManipulatorAxis::Y | ManipulatorAxis::Z => {
                    let amount = projected_drag_amount(
                        pointer_delta,
                        manipulator_direction(
                            drag_origin_transform,
                            manipulator_space,
                            manipulator_axis,
                        ),
                        camera_right,
                        camera_up,
                    ) * 0.008;
                    let rotation_delta = match manipulator_axis {
                        ManipulatorAxis::X => Vec3::new(amount, 0.0, 0.0),
                        ManipulatorAxis::Y => Vec3::new(0.0, amount, 0.0),
                        ManipulatorAxis::Z => Vec3::new(0.0, 0.0, amount),
                        _ => Vec3::ZERO,
                    };
                    updated_transform.rotation_radians =
                        drag_origin_transform.rotation_radians + rotation_delta;
                }
                ManipulatorAxis::PlaneXY | ManipulatorAxis::PlaneXZ | ManipulatorAxis::PlaneYZ => {
                    let amount = (pointer_delta.x - pointer_delta.y) * 0.006;
                    let rotation_delta = match manipulator_axis {
                        ManipulatorAxis::PlaneXY => Vec3::new(0.0, 0.0, amount),
                        ManipulatorAxis::PlaneXZ => Vec3::new(0.0, amount, 0.0),
                        ManipulatorAxis::PlaneYZ => Vec3::new(amount, 0.0, 0.0),
                        _ => Vec3::ZERO,
                    };
                    updated_transform.rotation_radians =
                        drag_origin_transform.rotation_radians + rotation_delta;
                }
            }
            if snap_enabled {
                if let Some(step) = snap_settings.rotation_step_radians {
                    updated_transform.rotation_radians =
                        snap_vec3(updated_transform.rotation_radians, step);
                }
            }
        }
        ManipulatorMode::Scale => {
            let base_scale = drag_origin_transform.scale;
            match manipulator_axis {
                ManipulatorAxis::Screen => {
                    let scale_factor =
                        (1.0 + (pointer_delta.x - pointer_delta.y) * 0.004).clamp(0.25, 5.0);
                    updated_transform.scale = base_scale * scale_factor;
                }
                ManipulatorAxis::X | ManipulatorAxis::Y | ManipulatorAxis::Z => {
                    let amount = projected_drag_amount(
                        pointer_delta,
                        manipulator_direction(
                            drag_origin_transform,
                            manipulator_space,
                            manipulator_axis,
                        ),
                        camera_right,
                        camera_up,
                    ) * 0.004;
                    let scale_factor = (1.0 + amount).clamp(0.25, 5.0);
                    let axis_scale = match manipulator_axis {
                        ManipulatorAxis::X => Vec3::new(scale_factor, 1.0, 1.0),
                        ManipulatorAxis::Y => Vec3::new(1.0, scale_factor, 1.0),
                        ManipulatorAxis::Z => Vec3::new(1.0, 1.0, scale_factor),
                        _ => Vec3::new(1.0, 1.0, 1.0),
                    };
                    updated_transform.scale = base_scale.component_mul(axis_scale);
                }
                ManipulatorAxis::PlaneXY | ManipulatorAxis::PlaneXZ | ManipulatorAxis::PlaneYZ => {
                    let scale_factor =
                        (1.0 + (pointer_delta.x - pointer_delta.y) * 0.0035).clamp(0.25, 5.0);
                    let plane_scale = match manipulator_axis {
                        ManipulatorAxis::PlaneXY => Vec3::new(scale_factor, scale_factor, 1.0),
                        ManipulatorAxis::PlaneXZ => Vec3::new(scale_factor, 1.0, scale_factor),
                        ManipulatorAxis::PlaneYZ => Vec3::new(1.0, scale_factor, scale_factor),
                        _ => Vec3::new(1.0, 1.0, 1.0),
                    };
                    updated_transform.scale = base_scale.component_mul(plane_scale);
                }
            }
            if snap_enabled {
                if let Some(step) = snap_settings.scale_step {
                    updated_transform.scale = Vec3::new(
                        round_to_step(updated_transform.scale.x, step).max(step),
                        round_to_step(updated_transform.scale.y, step).max(step),
                        round_to_step(updated_transform.scale.z, step).max(step),
                    );
                }
            }
        }
    }

    updated_transform
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
    pub fn pick_scene_with_overrides(
        &self,
        scene: &SceneDescription,
        query: &PickingQuery,
        instance_transform_overrides: &BTreeMap<String, Transform>,
    ) -> Option<PickingHit> {
        self.pick_scene_filtered_with_overrides(scene, query, None, instance_transform_overrides)
    }

    pub fn pick_catalog_scene_with_overrides(
        &self,
        catalog: &SceneCatalog,
        scene_name: &str,
        query: &PickingQuery,
        instance_transform_overrides: &BTreeMap<String, Transform>,
    ) -> Option<PickingHit> {
        let scene = catalog.scene(scene_name)?;
        self.pick_scene_with_overrides(scene, query, instance_transform_overrides)
    }

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

    pub fn pick_scene_instance_with_overrides(
        &self,
        scene: &SceneDescription,
        query: &PickingQuery,
        instance_id: &str,
        instance_transform_overrides: &BTreeMap<String, Transform>,
    ) -> Option<PickingHit> {
        self.pick_scene_filtered_with_overrides(
            scene,
            query,
            Some(instance_id),
            instance_transform_overrides,
        )
    }

    pub fn pick_catalog_scene_instance_with_overrides(
        &self,
        catalog: &SceneCatalog,
        scene_name: &str,
        query: &PickingQuery,
        instance_id: &str,
        instance_transform_overrides: &BTreeMap<String, Transform>,
    ) -> Option<PickingHit> {
        let scene = catalog.scene(scene_name)?;
        self.pick_scene_instance_with_overrides(
            scene,
            query,
            instance_id,
            instance_transform_overrides,
        )
    }

    fn pick_scene_filtered(
        &self,
        scene: &SceneDescription,
        query: &PickingQuery,
        instance_filter: Option<&str>,
    ) -> Option<PickingHit> {
        self.pick_scene_filtered_with_overrides(scene, query, instance_filter, &BTreeMap::new())
    }

    fn pick_scene_filtered_with_overrides(
        &self,
        scene: &SceneDescription,
        query: &PickingQuery,
        instance_filter: Option<&str>,
        instance_transform_overrides: &BTreeMap<String, Transform>,
    ) -> Option<PickingHit> {
        let mut closest_hit: Option<PickingHit> = None;

        for instance in scene.animated_instances_with_overrides(
            query.scene_time_seconds,
            instance_transform_overrides,
        ) {
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

fn manipulator_direction(
    drag_origin_transform: &Transform,
    manipulator_space: ManipulatorSpace,
    manipulator_axis: ManipulatorAxis,
) -> Vec3 {
    let axis = match manipulator_axis {
        ManipulatorAxis::X => Vec3::new(1.0, 0.0, 0.0),
        ManipulatorAxis::Y => Vec3::new(0.0, 1.0, 0.0),
        ManipulatorAxis::Z => Vec3::new(0.0, 0.0, 1.0),
        _ => return Vec3::ZERO,
    };

    match manipulator_space {
        ManipulatorSpace::World => axis,
        ManipulatorSpace::Local => Mat4::rotation_xyz(drag_origin_transform.rotation_radians)
            .transform_vector(axis)
            .normalize(),
    }
}

fn manipulator_plane_basis(
    drag_origin_transform: &Transform,
    manipulator_space: ManipulatorSpace,
    manipulator_axis: ManipulatorAxis,
) -> (Vec3, Vec3) {
    let (axis_u, axis_v) = match manipulator_axis {
        ManipulatorAxis::PlaneXY => (ManipulatorAxis::X, ManipulatorAxis::Y),
        ManipulatorAxis::PlaneXZ => (ManipulatorAxis::X, ManipulatorAxis::Z),
        ManipulatorAxis::PlaneYZ => (ManipulatorAxis::Y, ManipulatorAxis::Z),
        _ => return (Vec3::ZERO, Vec3::ZERO),
    };
    (
        manipulator_direction(drag_origin_transform, manipulator_space, axis_u),
        manipulator_direction(drag_origin_transform, manipulator_space, axis_v),
    )
}

fn projected_drag_amount(
    pointer_delta: Vec2,
    world_axis: Vec3,
    camera_right: Vec3,
    camera_up: Vec3,
) -> f32 {
    let projected = Vec2::new(world_axis.dot(camera_right), -world_axis.dot(camera_up));
    let fallback = if projected.length() <= f32::EPSILON {
        Vec2::new(1.0, 0.0)
    } else {
        projected.normalize()
    };
    pointer_delta.dot(fallback)
}

fn round_to_step(value: f32, step: f32) -> f32 {
    if step.abs() <= f32::EPSILON {
        value
    } else {
        (value / step).round() * step
    }
}

fn snap_vec3(value: Vec3, step: f32) -> Vec3 {
    Vec3::new(
        round_to_step(value.x, step),
        round_to_step(value.y, step),
        round_to_step(value.z, step),
    )
}

#[cfg(test)]
mod tests {
    use std::collections::BTreeMap;

    use crate::{
        BackgroundGradient, Camera, ColorRgb, DirectionalLight, Geometry, LightingRig, Material,
        PointLight, SceneDescription, SceneInstance, Transform, Vec2, Vec3,
    };

    use super::*;

    fn authored_block_geometry(size: Vec3) -> Geometry {
        let half = size * 0.5;
        Geometry::triangle_mesh()
            .with_positions(vec![
                Vec3::new(-half.x, -half.y, -half.z),
                Vec3::new(half.x, -half.y, -half.z),
                Vec3::new(half.x, half.y, -half.z),
                Vec3::new(-half.x, half.y, -half.z),
                Vec3::new(-half.x, -half.y, half.z),
                Vec3::new(half.x, -half.y, half.z),
                Vec3::new(half.x, half.y, half.z),
                Vec3::new(-half.x, half.y, half.z),
            ])
            .with_indices(vec![
                0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7, 0, 1, 5, 0, 5, 4, 3, 7, 6, 3, 6, 2, 1, 2, 6, 1,
                6, 5, 0, 4, 7, 0, 7, 3,
            ])
    }

    fn single_authored_block_scene() -> SceneDescription {
        let mesh = authored_block_geometry(Vec3::new(2.0, 2.0, 2.0))
            .to_mesh()
            .expect("authored mesh should build");
        let mut meshes = BTreeMap::new();
        meshes.insert("authored_block".to_string(), mesh);

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
                mesh: "authored_block".to_string(),
                material: "matte".to_string(),
                transform: Transform::identity(),
            }],
            animations: vec![],
            particle_emitters: vec![],
        }
    }

    #[test]
    fn center_ray_hits_authored_block() {
        let scene = single_authored_block_scene();
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
    fn offscreen_ray_misses_authored_block() {
        let scene = single_authored_block_scene();
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

    #[test]
    fn screen_translate_drag_updates_world_position() {
        let camera = CameraPose {
            position: Vec3::new(0.0, 4.0, 10.0),
            target: Vec3::ZERO,
            up: Vec3::UP,
            fov_y_degrees: 60.0,
            near_plane: 0.1,
            far_plane: 100.0,
        };
        let origin = Transform::identity().with_translation(Vec3::new(1.0, 2.0, 3.0));

        let updated = apply_manipulator_drag(
            &camera,
            RenderResolution::new(1280, 720),
            &origin,
            Vec2::new(120.0, -48.0),
            ManipulatorMode::Translate,
            ManipulatorSpace::World,
            ManipulatorAxis::Screen,
            false,
            ManipulatorSnapSettings::default(),
        );

        assert!(updated.translation.x > origin.translation.x);
        assert!(updated.translation.y > origin.translation.y);
    }

    #[test]
    fn axis_scale_drag_remains_positive() {
        let camera = CameraPose {
            position: Vec3::new(0.0, 4.0, 10.0),
            target: Vec3::ZERO,
            up: Vec3::UP,
            fov_y_degrees: 60.0,
            near_plane: 0.1,
            far_plane: 100.0,
        };
        let origin = Transform::identity().with_scale(Vec3::new(2.0, 3.0, 4.0));

        let updated = apply_manipulator_drag(
            &camera,
            RenderResolution::new(1280, 720),
            &origin,
            Vec2::new(-900.0, 900.0),
            ManipulatorMode::Scale,
            ManipulatorSpace::World,
            ManipulatorAxis::X,
            false,
            ManipulatorSnapSettings::default(),
        );

        assert!(updated.scale.x > 0.0);
        assert_eq!(updated.scale.y, origin.scale.y);
        assert_eq!(updated.scale.z, origin.scale.z);
    }

    #[test]
    fn translate_drag_snaps_when_enabled() {
        let camera = CameraPose {
            position: Vec3::new(0.0, 4.0, 10.0),
            target: Vec3::ZERO,
            up: Vec3::UP,
            fov_y_degrees: 60.0,
            near_plane: 0.1,
            far_plane: 100.0,
        };
        let origin = Transform::identity().with_translation(Vec3::new(0.3, 0.7, 1.1));

        let updated = apply_manipulator_drag(
            &camera,
            RenderResolution::new(1280, 720),
            &origin,
            Vec2::new(67.0, -19.0),
            ManipulatorMode::Translate,
            ManipulatorSpace::World,
            ManipulatorAxis::Screen,
            true,
            ManipulatorSnapSettings {
                translation_step: Some(0.5),
                rotation_step_radians: None,
                scale_step: None,
            },
        );

        assert!((updated.translation.x * 2.0).fract().abs() <= 1.0e-4);
        assert!((updated.translation.y * 2.0).fract().abs() <= 1.0e-4);
    }

    #[test]
    fn local_axis_translation_respects_object_rotation() {
        let camera = CameraPose {
            position: Vec3::new(6.0, 3.0, 6.0),
            target: Vec3::ZERO,
            up: Vec3::UP,
            fov_y_degrees: 55.0,
            near_plane: 0.1,
            far_plane: 100.0,
        };
        let origin = Transform::identity()
            .with_translation(Vec3::new(0.0, 0.0, 0.0))
            .with_rotation(Vec3::new(0.0, std::f32::consts::FRAC_PI_2, 0.0));

        let updated = apply_manipulator_drag(
            &camera,
            RenderResolution::new(1280, 720),
            &origin,
            Vec2::new(140.0, 0.0),
            ManipulatorMode::Translate,
            ManipulatorSpace::Local,
            ManipulatorAxis::X,
            false,
            ManipulatorSnapSettings::default(),
        );

        assert!(updated.translation.z.abs() > 0.01);
        assert!(updated.translation.x.abs() < updated.translation.z.abs());
    }
}
