use std::{collections::BTreeMap, f32::consts::TAU};

use crate::authoring::{Geometry, GeometryError, Scene};
use crate::scene::Mesh;
use crate::{Vec2, Vec3};

const AUTHORED_PRIMITIVE_DOCUMENT_URI: &str = "mesh://primitives/authored/definitions";

#[derive(Clone, Debug, PartialEq)]
pub enum PrimitiveShape {
    Plane {
        size: Vec2,
        width_segments: usize,
        depth_segments: usize,
    },
    Box {
        size: Vec3,
        width_segments: usize,
        height_segments: usize,
        depth_segments: usize,
    },
    UvSphere {
        radius: f32,
        latitude_segments: usize,
        longitude_segments: usize,
    },
    QuadSphere {
        radius: f32,
        resolution: usize,
    },
    Cylinder {
        radius: f32,
        height: f32,
        radial_segments: usize,
        height_segments: usize,
        cap_segments: usize,
    },
    Cone {
        radius: f32,
        height: f32,
        radial_segments: usize,
        height_segments: usize,
        cap_segments: usize,
    },
    Capsule {
        radius: f32,
        height: f32,
        radial_segments: usize,
        hemisphere_segments: usize,
        body_segments: usize,
    },
    Torus {
        major_radius: f32,
        minor_radius: f32,
        major_segments: usize,
        minor_segments: usize,
    },
}

impl PrimitiveShape {
    pub fn build_geometry(&self) -> Geometry {
        match self {
            Self::Plane {
                size,
                width_segments,
                depth_segments,
            } => build_plane_geometry(*size, *width_segments, *depth_segments),
            Self::Box {
                size,
                width_segments,
                height_segments,
                depth_segments,
            } => build_box_geometry(*size, *width_segments, *height_segments, *depth_segments),
            Self::UvSphere {
                radius,
                latitude_segments,
                longitude_segments,
            } => build_uv_sphere_geometry(*radius, *latitude_segments, *longitude_segments),
            Self::QuadSphere { radius, resolution } => {
                build_quad_sphere_geometry(*radius, *resolution)
            }
            Self::Cylinder {
                radius,
                height,
                radial_segments,
                height_segments,
                cap_segments,
            } => build_cylinder_geometry(
                *radius,
                *height,
                *radial_segments,
                *height_segments,
                *cap_segments,
            ),
            Self::Cone {
                radius,
                height,
                radial_segments,
                height_segments,
                cap_segments,
            } => build_cone_geometry(
                *radius,
                *height,
                *radial_segments,
                *height_segments,
                *cap_segments,
            ),
            Self::Capsule {
                radius,
                height,
                radial_segments,
                hemisphere_segments,
                body_segments,
            } => build_capsule_geometry(
                *radius,
                *height,
                *radial_segments,
                *hemisphere_segments,
                *body_segments,
            ),
            Self::Torus {
                major_radius,
                minor_radius,
                major_segments,
                minor_segments,
            } => build_torus_geometry(
                *major_radius,
                *minor_radius,
                *major_segments,
                *minor_segments,
            ),
        }
    }

    pub fn build_mesh(&self) -> Result<Mesh, GeometryError> {
        self.build_geometry().to_mesh()
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct PrimitiveDefinition {
    pub id: String,
    pub resource_uri: String,
    pub display_name: String,
    pub shape: PrimitiveShape,
    pub subdivision_ready: bool,
    pub authored_intent: String,
}

impl PrimitiveDefinition {
    pub fn authored(
        id: impl Into<String>,
        display_name: impl Into<String>,
        shape: PrimitiveShape,
        subdivision_ready: bool,
        authored_intent: impl Into<String>,
    ) -> Self {
        let id = id.into();
        Self {
            resource_uri: format!("mesh://primitives/authored/{id}"),
            id,
            display_name: display_name.into(),
            shape,
            subdivision_ready,
            authored_intent: authored_intent.into(),
        }
    }

    pub fn build_geometry(&self) -> Geometry {
        self.shape.build_geometry()
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct PrimitiveLibrary {
    pub resource_document_uri: String,
    pub startup_primitive_id: String,
    pub authored_policy: String,
    pub definitions: BTreeMap<String, PrimitiveDefinition>,
}

impl PrimitiveLibrary {
    pub fn authored_defaults() -> Self {
        let mut definitions = BTreeMap::new();
        let startup_cube = PrimitiveDefinition::authored(
            "startup-cube",
            "Startup Cube",
            PrimitiveShape::Box {
                size: Vec3::new(2.0, 2.0, 2.0),
                width_segments: 1,
                height_segments: 1,
                depth_segments: 1,
            },
            true,
            "baseline DCC startup primitive with clean face normals and stable authored identity",
        );
        definitions.insert(startup_cube.id.clone(), startup_cube);
        let studio_plane = PrimitiveDefinition::authored(
            "studio-plane",
            "Studio Plane",
            PrimitiveShape::Plane {
                size: Vec2::new(8.0, 8.0),
                width_segments: 8,
                depth_segments: 8,
            },
            false,
            "subdivided staging plane for sculpt, lookdev, and blocking passes",
        );
        definitions.insert(studio_plane.id.clone(), studio_plane);
        let studio_cylinder = PrimitiveDefinition::authored(
            "studio-cylinder",
            "Studio Cylinder",
            PrimitiveShape::Cylinder {
                radius: 1.0,
                height: 2.4,
                radial_segments: 48,
                height_segments: 6,
                cap_segments: 3,
            },
            true,
            "high-fidelity round primitive with support loops suitable for subdivision workflows",
        );
        definitions.insert(studio_cylinder.id.clone(), studio_cylinder);
        let sculpt_capsule = PrimitiveDefinition::authored(
            "sculpt-capsule",
            "Sculpt Capsule",
            PrimitiveShape::Capsule {
                radius: 0.8,
                height: 3.2,
                radial_segments: 48,
                hemisphere_segments: 12,
                body_segments: 5,
            },
            true,
            "organic blocking primitive for character and creature sculpt passes",
        );
        definitions.insert(sculpt_capsule.id.clone(), sculpt_capsule);
        let hero_quad_sphere = PrimitiveDefinition::authored(
            "hero-quad-sphere",
            "Hero Quad Sphere",
            PrimitiveShape::QuadSphere {
                radius: 1.0,
                resolution: 10,
            },
            true,
            "evenly distributed sphere topology for sculpt, remesh, and subdivision-heavy authoring",
        );
        definitions.insert(hero_quad_sphere.id.clone(), hero_quad_sphere);
        let hero_torus = PrimitiveDefinition::authored(
            "hero-torus",
            "Hero Torus",
            PrimitiveShape::Torus {
                major_radius: 1.1,
                minor_radius: 0.32,
                major_segments: 72,
                minor_segments: 28,
            },
            true,
            "dense torus primitive for hard-surface blocking, kitbash details, and deformation tests",
        );
        definitions.insert(hero_torus.id.clone(), hero_torus);
        let hero_cone = PrimitiveDefinition::authored(
            "hero-cone",
            "Hero Cone",
            PrimitiveShape::Cone {
                radius: 1.0,
                height: 2.4,
                radial_segments: 48,
                height_segments: 5,
                cap_segments: 2,
            },
            false,
            "clean tapered primitive for staging, motion graphics, and profile-based modeling starts",
        );
        definitions.insert(hero_cone.id.clone(), hero_cone);
        let hero_uv_sphere = PrimitiveDefinition::authored(
            "hero-uv-sphere",
            "Hero UV Sphere",
            PrimitiveShape::UvSphere {
                radius: 1.0,
                latitude_segments: 32,
                longitude_segments: 64,
            },
            true,
            "high-density UV sphere for predictable latitude-longitude layouts and texture tests",
        );
        definitions.insert(hero_uv_sphere.id.clone(), hero_uv_sphere);
        Self {
            resource_document_uri: AUTHORED_PRIMITIVE_DOCUMENT_URI.to_string(),
            startup_primitive_id: "startup-cube".to_string(),
            authored_policy: "primitives_first_then_imports".to_string(),
            definitions,
        }
    }

    pub fn definition(&self, id: &str) -> Option<&PrimitiveDefinition> {
        self.definitions.get(id)
    }

    pub fn summary(&self) -> String {
        let subdivision_ready_count = self
            .definitions
            .values()
            .filter(|definition| definition.subdivision_ready)
            .count();
        let startup_name = self
            .definition(&self.startup_primitive_id)
            .map(|definition| definition.display_name.as_str())
            .unwrap_or(self.startup_primitive_id.as_str());
        format!(
            "authored catalog: {} primitives, {} subdivision-ready, startup {} ({}), policy {}",
            self.definitions.len(),
            subdivision_ready_count,
            startup_name,
            self.startup_primitive_id,
            self.authored_policy
        )
    }

    pub fn geometry_map(&self) -> BTreeMap<String, Geometry> {
        self.definitions
            .iter()
            .map(|(id, definition)| (id.clone(), definition.build_geometry()))
            .collect()
    }

    pub fn register_into_scene<'a>(&self, scene: &'a mut Scene) -> &'a mut Scene {
        scene.add_primitive_library(self)
    }
}

fn build_plane_geometry(size: Vec2, width_segments: usize, depth_segments: usize) -> Geometry {
    let half_width = size.x * 0.5;
    let half_depth = size.y * 0.5;
    let mut positions = Vec::new();
    let mut normals = Vec::new();
    let mut uvs = Vec::new();
    let mut indices = Vec::new();
    append_grid_patch(
        &mut positions,
        &mut normals,
        &mut uvs,
        &mut indices,
        Vec3::new(-half_width, 0.0, half_depth),
        Vec3::new(size.x, 0.0, 0.0),
        Vec3::new(0.0, 0.0, -size.y),
        width_segments.max(1),
        depth_segments.max(1),
        |point| (point, Vec3::UP),
    );
    Geometry::triangle_mesh()
        .with_positions(positions)
        .with_normals(normals)
        .with_uvs(uvs)
        .with_indices(indices)
}

fn build_box_geometry(
    size: Vec3,
    width_segments: usize,
    height_segments: usize,
    depth_segments: usize,
) -> Geometry {
    let half_width = size.x * 0.5;
    let half_height = size.y * 0.5;
    let half_depth = size.z * 0.5;
    let width_segments = width_segments.max(1);
    let height_segments = height_segments.max(1);
    let depth_segments = depth_segments.max(1);
    let mut positions = Vec::new();
    let mut normals = Vec::new();
    let mut uvs = Vec::new();
    let mut indices = Vec::new();
    append_grid_patch(
        &mut positions,
        &mut normals,
        &mut uvs,
        &mut indices,
        Vec3::new(-half_width, -half_height, half_depth),
        Vec3::new(size.x, 0.0, 0.0),
        Vec3::new(0.0, size.y, 0.0),
        width_segments,
        height_segments,
        |point| (point, Vec3::new(0.0, 0.0, 1.0)),
    );
    append_grid_patch(
        &mut positions,
        &mut normals,
        &mut uvs,
        &mut indices,
        Vec3::new(half_width, -half_height, -half_depth),
        Vec3::new(-size.x, 0.0, 0.0),
        Vec3::new(0.0, size.y, 0.0),
        width_segments,
        height_segments,
        |point| (point, Vec3::new(0.0, 0.0, -1.0)),
    );
    append_grid_patch(
        &mut positions,
        &mut normals,
        &mut uvs,
        &mut indices,
        Vec3::new(-half_width, -half_height, -half_depth),
        Vec3::new(0.0, 0.0, size.z),
        Vec3::new(0.0, size.y, 0.0),
        depth_segments,
        height_segments,
        |point| (point, Vec3::new(-1.0, 0.0, 0.0)),
    );
    append_grid_patch(
        &mut positions,
        &mut normals,
        &mut uvs,
        &mut indices,
        Vec3::new(half_width, -half_height, half_depth),
        Vec3::new(0.0, 0.0, -size.z),
        Vec3::new(0.0, size.y, 0.0),
        depth_segments,
        height_segments,
        |point| (point, Vec3::new(1.0, 0.0, 0.0)),
    );
    append_grid_patch(
        &mut positions,
        &mut normals,
        &mut uvs,
        &mut indices,
        Vec3::new(-half_width, half_height, half_depth),
        Vec3::new(size.x, 0.0, 0.0),
        Vec3::new(0.0, 0.0, -size.z),
        width_segments,
        depth_segments,
        |point| (point, Vec3::new(0.0, 1.0, 0.0)),
    );
    append_grid_patch(
        &mut positions,
        &mut normals,
        &mut uvs,
        &mut indices,
        Vec3::new(-half_width, -half_height, -half_depth),
        Vec3::new(size.x, 0.0, 0.0),
        Vec3::new(0.0, 0.0, size.z),
        width_segments,
        depth_segments,
        |point| (point, Vec3::new(0.0, -1.0, 0.0)),
    );
    Geometry::triangle_mesh()
        .with_positions(positions)
        .with_normals(normals)
        .with_uvs(uvs)
        .with_indices(indices)
}

fn build_uv_sphere_geometry(
    radius: f32,
    latitude_segments: usize,
    longitude_segments: usize,
) -> Geometry {
    let latitude_segments = latitude_segments.max(3);
    let longitude_segments = longitude_segments.max(4);
    let mut positions = Vec::new();
    let mut normals = Vec::new();
    let mut uvs = Vec::new();
    let mut indices = Vec::new();
    append_parametric_surface(
        &mut positions,
        &mut normals,
        &mut uvs,
        &mut indices,
        longitude_segments,
        latitude_segments,
        false,
        |u, v| {
            let theta = u * TAU;
            let phi = v * std::f32::consts::PI;
            let normal = Vec3::new(theta.cos() * phi.sin(), phi.cos(), theta.sin() * phi.sin());
            (normal * radius, normal.normalize(), Vec2::new(u, v))
        },
    );
    Geometry::triangle_mesh()
        .with_positions(positions)
        .with_normals(normals)
        .with_uvs(uvs)
        .with_indices(indices)
}

fn build_quad_sphere_geometry(radius: f32, resolution: usize) -> Geometry {
    let resolution = resolution.max(1);
    let mut positions = Vec::new();
    let mut normals = Vec::new();
    let mut uvs = Vec::new();
    let mut indices = Vec::new();
    for (origin, u_axis, v_axis) in quad_sphere_faces() {
        append_grid_patch(
            &mut positions,
            &mut normals,
            &mut uvs,
            &mut indices,
            origin,
            u_axis,
            v_axis,
            resolution,
            resolution,
            |raw_point| {
                let normal = raw_point.normalize();
                (normal * radius, normal)
            },
        );
    }
    Geometry::triangle_mesh()
        .with_positions(positions)
        .with_normals(normals)
        .with_uvs(uvs)
        .with_indices(indices)
}

fn build_cylinder_geometry(
    radius: f32,
    height: f32,
    radial_segments: usize,
    height_segments: usize,
    cap_segments: usize,
) -> Geometry {
    let radial_segments = radial_segments.max(3);
    let height_segments = height_segments.max(1);
    let cap_segments = cap_segments.max(1);
    let half_height = height * 0.5;
    let mut positions = Vec::new();
    let mut normals = Vec::new();
    let mut uvs = Vec::new();
    let mut indices = Vec::new();
    append_parametric_surface(
        &mut positions,
        &mut normals,
        &mut uvs,
        &mut indices,
        radial_segments,
        height_segments,
        false,
        |u, v| {
            let theta = u * TAU;
            let y = half_height - v * height;
            let normal = Vec3::new(theta.cos(), 0.0, theta.sin()).normalize();
            (
                Vec3::new(theta.cos() * radius, y, theta.sin() * radius),
                normal,
                Vec2::new(u, v),
            )
        },
    );
    append_disk_geometry(
        &mut positions,
        &mut normals,
        &mut uvs,
        &mut indices,
        radius,
        half_height,
        radial_segments,
        cap_segments,
        false,
    );
    append_disk_geometry(
        &mut positions,
        &mut normals,
        &mut uvs,
        &mut indices,
        radius,
        -half_height,
        radial_segments,
        cap_segments,
        true,
    );
    Geometry::triangle_mesh()
        .with_positions(positions)
        .with_normals(normals)
        .with_uvs(uvs)
        .with_indices(indices)
}

fn build_cone_geometry(
    radius: f32,
    height: f32,
    radial_segments: usize,
    height_segments: usize,
    cap_segments: usize,
) -> Geometry {
    let radial_segments = radial_segments.max(3);
    let height_segments = height_segments.max(1);
    let cap_segments = cap_segments.max(1);
    let half_height = height * 0.5;
    let slope = radius / height.max(f32::EPSILON);
    let mut positions = Vec::new();
    let mut normals = Vec::new();
    let mut uvs = Vec::new();
    let mut indices = Vec::new();
    let ring_count = height_segments;
    for ring_index in 0..ring_count {
        let t = (ring_index + 1) as f32 / ring_count as f32;
        let current_radius = radius * t;
        let y = half_height - t * height;
        for radial in 0..=radial_segments {
            let u = radial as f32 / radial_segments as f32;
            let theta = u * TAU;
            positions.push(Vec3::new(
                theta.cos() * current_radius,
                y,
                theta.sin() * current_radius,
            ));
            normals.push(Vec3::new(theta.cos(), slope, theta.sin()).normalize());
            uvs.push(Vec2::new(u, t));
        }
    }
    let ring_stride = radial_segments + 1;
    for ring_index in 0..ring_count.saturating_sub(1) {
        let base = (ring_index * ring_stride) as u32;
        for radial in 0..radial_segments {
            let a = base + radial as u32;
            let b = a + 1;
            let c = a + ring_stride as u32;
            let d = c + 1;
            indices.extend([a, b, c, b, d, c]);
        }
    }
    let apex_y = half_height;
    let apex_base = positions.len() as u32;
    for radial in 0..radial_segments {
        let u = radial as f32 / radial_segments as f32;
        let theta0 = u * TAU;
        let theta1 = (radial + 1) as f32 / radial_segments as f32 * TAU;
        positions.push(Vec3::new(0.0, apex_y, 0.0));
        let averaged = Vec3::new(
            (theta0.cos() + theta1.cos()) * 0.5,
            slope,
            (theta0.sin() + theta1.sin()) * 0.5,
        )
        .normalize();
        normals.push(averaged);
        uvs.push(Vec2::new(u + 0.5 / radial_segments as f32, 0.0));
    }
    let first_ring_base = 0u32;
    for radial in 0..radial_segments {
        let ring_vertex = first_ring_base + radial as u32;
        let next_ring_vertex = ring_vertex + 1;
        let apex = apex_base + radial as u32;
        indices.extend([apex, next_ring_vertex, ring_vertex]);
    }
    append_disk_geometry(
        &mut positions,
        &mut normals,
        &mut uvs,
        &mut indices,
        radius,
        -half_height,
        radial_segments,
        cap_segments,
        true,
    );
    Geometry::triangle_mesh()
        .with_positions(positions)
        .with_normals(normals)
        .with_uvs(uvs)
        .with_indices(indices)
}

fn build_capsule_geometry(
    radius: f32,
    height: f32,
    radial_segments: usize,
    hemisphere_segments: usize,
    body_segments: usize,
) -> Geometry {
    let radial_segments = radial_segments.max(3);
    let hemisphere_segments = hemisphere_segments.max(2);
    let body_segments = body_segments.max(1);
    let straight_height = (height - radius * 2.0).max(0.0);
    let half_straight = straight_height * 0.5;
    let total_rows = hemisphere_segments * 2 + body_segments;
    let mut positions = Vec::new();
    let mut normals = Vec::new();
    let mut uvs = Vec::new();
    let mut indices = Vec::new();
    append_parametric_surface(
        &mut positions,
        &mut normals,
        &mut uvs,
        &mut indices,
        radial_segments,
        total_rows,
        false,
        |u, v| {
            let theta = u * TAU;
            let row = v * total_rows as f32;
            let top_hemi_end = hemisphere_segments as f32;
            let body_end = top_hemi_end + body_segments as f32;
            let (center_y, phi) = if row <= top_hemi_end {
                let t = row / hemisphere_segments as f32;
                (half_straight, t * std::f32::consts::FRAC_PI_2)
            } else if row <= body_end {
                let t = (row - top_hemi_end) / body_segments as f32;
                let y = half_straight - t * straight_height;
                (y, std::f32::consts::FRAC_PI_2)
            } else {
                let t = (row - body_end) / hemisphere_segments as f32;
                (
                    -half_straight,
                    std::f32::consts::FRAC_PI_2 + t * std::f32::consts::FRAC_PI_2,
                )
            };
            let radial = phi.sin();
            let normal_y = phi.cos();
            let normal =
                Vec3::new(theta.cos() * radial, normal_y, theta.sin() * radial).normalize();
            let local_y = if row <= top_hemi_end {
                phi.cos() * radius
            } else if row <= body_end {
                0.0
            } else {
                phi.cos() * radius
            };
            let y = center_y + local_y;
            (
                Vec3::new(
                    theta.cos() * radial * radius,
                    y,
                    theta.sin() * radial * radius,
                ),
                normal,
                Vec2::new(u, v),
            )
        },
    );
    Geometry::triangle_mesh()
        .with_positions(positions)
        .with_normals(normals)
        .with_uvs(uvs)
        .with_indices(indices)
}

fn build_torus_geometry(
    major_radius: f32,
    minor_radius: f32,
    major_segments: usize,
    minor_segments: usize,
) -> Geometry {
    let major_segments = major_segments.max(3);
    let minor_segments = minor_segments.max(3);
    let mut positions = Vec::new();
    let mut normals = Vec::new();
    let mut uvs = Vec::new();
    let mut indices = Vec::new();
    append_parametric_surface(
        &mut positions,
        &mut normals,
        &mut uvs,
        &mut indices,
        major_segments,
        minor_segments,
        false,
        |u, v| {
            let major_angle = u * TAU;
            let minor_angle = (1.0 - v) * TAU;
            let ring_center = Vec3::new(
                major_angle.cos() * major_radius,
                0.0,
                major_angle.sin() * major_radius,
            );
            let outward = Vec3::new(major_angle.cos(), 0.0, major_angle.sin());
            let up = Vec3::UP;
            let normal = (outward * minor_angle.cos() + up * minor_angle.sin()).normalize();
            (ring_center + normal * minor_radius, normal, Vec2::new(u, v))
        },
    );
    Geometry::triangle_mesh()
        .with_positions(positions)
        .with_normals(normals)
        .with_uvs(uvs)
        .with_indices(indices)
}

fn append_grid_patch<F>(
    positions: &mut Vec<Vec3>,
    normals: &mut Vec<Vec3>,
    uvs: &mut Vec<Vec2>,
    indices: &mut Vec<u32>,
    origin: Vec3,
    u_axis: Vec3,
    v_axis: Vec3,
    u_segments: usize,
    v_segments: usize,
    map_position: F,
) where
    F: Fn(Vec3) -> (Vec3, Vec3),
{
    let u_segments = u_segments.max(1);
    let v_segments = v_segments.max(1);
    let base = positions.len() as u32;
    for v_index in 0..=v_segments {
        let v = v_index as f32 / v_segments as f32;
        for u_index in 0..=u_segments {
            let u = u_index as f32 / u_segments as f32;
            let raw = origin + u_axis * u + v_axis * v;
            let (position, normal) = map_position(raw);
            positions.push(position);
            normals.push(normal.normalize());
            uvs.push(Vec2::new(u, v));
        }
    }
    append_grid_indices(indices, base, u_segments, v_segments, false);
}

fn append_parametric_surface<F>(
    positions: &mut Vec<Vec3>,
    normals: &mut Vec<Vec3>,
    uvs: &mut Vec<Vec2>,
    indices: &mut Vec<u32>,
    u_segments: usize,
    v_segments: usize,
    flip_winding: bool,
    sample: F,
) where
    F: Fn(f32, f32) -> (Vec3, Vec3, Vec2),
{
    let u_segments = u_segments.max(1);
    let v_segments = v_segments.max(1);
    let base = positions.len() as u32;
    for v_index in 0..=v_segments {
        let v = v_index as f32 / v_segments as f32;
        for u_index in 0..=u_segments {
            let u = u_index as f32 / u_segments as f32;
            let (position, normal, uv) = sample(u, v);
            positions.push(position);
            normals.push(normal.normalize());
            uvs.push(uv);
        }
    }
    append_grid_indices(indices, base, u_segments, v_segments, flip_winding);
}

fn append_grid_indices(
    indices: &mut Vec<u32>,
    base: u32,
    u_segments: usize,
    v_segments: usize,
    flip_winding: bool,
) {
    let stride = u_segments + 1;
    for v_index in 0..v_segments {
        for u_index in 0..u_segments {
            let a = base + (v_index * stride + u_index) as u32;
            let b = a + 1;
            let c = a + stride as u32;
            let d = c + 1;
            if flip_winding {
                indices.extend([a, c, b, b, c, d]);
            } else {
                indices.extend([a, b, c, b, d, c]);
            }
        }
    }
}

fn append_disk_geometry(
    positions: &mut Vec<Vec3>,
    normals: &mut Vec<Vec3>,
    uvs: &mut Vec<Vec2>,
    indices: &mut Vec<u32>,
    radius: f32,
    y: f32,
    radial_segments: usize,
    ring_segments: usize,
    flip_winding: bool,
) {
    append_parametric_surface(
        positions,
        normals,
        uvs,
        indices,
        radial_segments.max(3),
        ring_segments.max(1),
        flip_winding,
        |u, v| {
            let theta = u * TAU;
            let current_radius = v * radius;
            let position = Vec3::new(
                theta.cos() * current_radius,
                y,
                theta.sin() * current_radius,
            );
            let normal = if flip_winding {
                Vec3::new(0.0, -1.0, 0.0)
            } else {
                Vec3::new(0.0, 1.0, 0.0)
            };
            let uv = Vec2::new(theta.cos() * v * 0.5 + 0.5, theta.sin() * v * 0.5 + 0.5);
            (position, normal, uv)
        },
    );
}

fn quad_sphere_faces() -> [(Vec3, Vec3, Vec3); 6] {
    [
        (
            Vec3::new(-1.0, -1.0, 1.0),
            Vec3::new(2.0, 0.0, 0.0),
            Vec3::new(0.0, 2.0, 0.0),
        ),
        (
            Vec3::new(1.0, -1.0, -1.0),
            Vec3::new(-2.0, 0.0, 0.0),
            Vec3::new(0.0, 2.0, 0.0),
        ),
        (
            Vec3::new(-1.0, -1.0, -1.0),
            Vec3::new(0.0, 0.0, 2.0),
            Vec3::new(0.0, 2.0, 0.0),
        ),
        (
            Vec3::new(1.0, -1.0, 1.0),
            Vec3::new(0.0, 0.0, -2.0),
            Vec3::new(0.0, 2.0, 0.0),
        ),
        (
            Vec3::new(-1.0, 1.0, 1.0),
            Vec3::new(2.0, 0.0, 0.0),
            Vec3::new(0.0, 0.0, -2.0),
        ),
        (
            Vec3::new(-1.0, -1.0, -1.0),
            Vec3::new(2.0, 0.0, 0.0),
            Vec3::new(0.0, 0.0, 2.0),
        ),
    ]
}

#[cfg(test)]
mod tests {
    use crate::{ColorRgb, Material};

    use super::*;

    #[test]
    fn authored_primitive_library_matches_mesh_contract_shape() {
        let library = PrimitiveLibrary::authored_defaults();
        assert_eq!(
            library.resource_document_uri,
            "mesh://primitives/authored/definitions"
        );
        assert_eq!(library.startup_primitive_id, "startup-cube");
        assert!(library.definitions.contains_key("hero-torus"));
        assert!(library
            .definition("hero-quad-sphere")
            .is_some_and(|definition| definition.subdivision_ready));
        assert_eq!(
            library.summary(),
            "authored catalog: 8 primitives, 6 subdivision-ready, startup Startup Cube (startup-cube), policy primitives_first_then_imports"
        );
    }

    #[test]
    fn advanced_primitives_build_into_renderable_meshes() {
        let shapes = [
            PrimitiveShape::Cylinder {
                radius: 1.0,
                height: 2.0,
                radial_segments: 32,
                height_segments: 4,
                cap_segments: 2,
            },
            PrimitiveShape::Cone {
                radius: 1.0,
                height: 2.0,
                radial_segments: 32,
                height_segments: 3,
                cap_segments: 2,
            },
            PrimitiveShape::Capsule {
                radius: 0.75,
                height: 3.0,
                radial_segments: 32,
                hemisphere_segments: 8,
                body_segments: 4,
            },
            PrimitiveShape::QuadSphere {
                radius: 1.0,
                resolution: 8,
            },
            PrimitiveShape::Torus {
                major_radius: 1.2,
                minor_radius: 0.35,
                major_segments: 48,
                minor_segments: 18,
            },
        ];
        for shape in shapes {
            let mesh = shape
                .build_mesh()
                .expect("primitive should convert into a mesh");
            assert!(!mesh.vertices.is_empty());
            assert!(!mesh.triangles.is_empty());
            assert!(mesh
                .vertices
                .iter()
                .all(|vertex| (vertex.normal.length() - 1.0).abs() < 0.001));
        }
    }

    #[test]
    fn primitive_library_registers_into_authoring_scene() {
        let library = PrimitiveLibrary::authored_defaults();
        let mut scene = Scene::new("primitive_authoring");
        scene
            .add_material("hero", Material::glossy(ColorRgb::new(0.2, 0.6, 0.9)))
            .add_primitive_library(&library)
            .spawn_mesh("hero", "startup-cube", "hero");
        assert_eq!(
            scene
                .metadata
                .get("primitive_library.resource_document_uri"),
            Some(&"mesh://primitives/authored/definitions".to_string())
        );
        assert_eq!(
            scene.metadata.get("primitive_library.definition_count"),
            Some(&library.definitions.len().to_string())
        );
        assert_eq!(
            scene
                .metadata
                .get("primitive_library.subdivision_ready_count"),
            Some(&"6".to_string())
        );
        assert_eq!(
            scene.metadata.get("primitive_library.summary"),
            Some(&"authored catalog: 8 primitives, 6 subdivision-ready, startup Startup Cube (startup-cube), policy primitives_first_then_imports".to_string())
        );
        assert_eq!(
            scene
                .metadata
                .get("primitive_library.startup_primitive_display_name"),
            Some(&"Startup Cube".to_string())
        );
        assert!(scene
            .metadata
            .get("primitive_library.definition_ids")
            .is_some_and(|ids| ids.contains("hero-torus") && ids.contains("startup-cube")));
        assert!(scene.geometries.contains_key("hero-torus"));
        let description = scene.flatten().expect("scene should flatten");
        assert_eq!(description.instances.len(), 1);
        assert!(description.meshes.contains_key("startup-cube"));
    }
}
