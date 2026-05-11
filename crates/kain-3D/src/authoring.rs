use std::collections::{BTreeMap, BTreeSet};

use crate::math::{Mat4, Vec2};
use crate::primitive::{PrimitiveDefinition, PrimitiveLibrary, PrimitiveShape};
use crate::scene::{
    BackgroundGradient, Camera, DirectionalLight, LightingRig, Material, Mesh, PointLight,
    SceneDescription, SceneInstance, Vertex,
};
use crate::{ColorRgb, Transform, Vec3};

#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub enum AttributeDomain {
    Vertex,
    Face,
    Instance,
}

#[derive(Clone, Debug, PartialEq)]
pub enum AttributeValues {
    Scalar(Vec<f32>),
    Vec2(Vec<Vec2>),
    Vec3(Vec<Vec3>),
    Color(Vec<ColorRgb>),
}

impl AttributeValues {
    pub fn len(&self) -> usize {
        match self {
            Self::Scalar(values) => values.len(),
            Self::Vec2(values) => values.len(),
            Self::Vec3(values) => values.len(),
            Self::Color(values) => values.len(),
        }
    }

    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct GeometryAttribute {
    pub name: String,
    pub domain: AttributeDomain,
    pub values: AttributeValues,
}

impl GeometryAttribute {
    pub fn new(name: impl Into<String>, domain: AttributeDomain, values: AttributeValues) -> Self {
        Self {
            name: name.into(),
            domain,
            values,
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum GeometryTopology {
    Triangles,
    Lines,
    Points,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum GeometryError {
    MissingPositions,
    InvalidAttributeLength {
        attribute: String,
        expected: usize,
        actual: usize,
    },
    InvalidIndex(u32),
    InvalidTriangleIndexCount(usize),
    UnsupportedTopology(GeometryTopology),
}

impl std::fmt::Display for GeometryError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::MissingPositions => write!(f, "geometry is missing a `position` attribute"),
            Self::InvalidAttributeLength {
                attribute,
                expected,
                actual,
            } => write!(
                f,
                "attribute `{attribute}` expected {expected} values but found {actual}"
            ),
            Self::InvalidIndex(index) => {
                write!(f, "geometry references invalid vertex index `{index}`")
            }
            Self::InvalidTriangleIndexCount(count) => write!(
                f,
                "triangle geometry requires an index count divisible by 3, found {count}"
            ),
            Self::UnsupportedTopology(topology) => {
                write!(f, "operation does not support topology `{topology:?}`")
            }
        }
    }
}

impl std::error::Error for GeometryError {}

#[derive(Clone, Debug, PartialEq)]
pub struct Geometry {
    pub topology: GeometryTopology,
    pub attributes: BTreeMap<String, GeometryAttribute>,
    pub indices: Vec<u32>,
}

impl Geometry {
    pub fn new(topology: GeometryTopology) -> Self {
        Self {
            topology,
            attributes: BTreeMap::new(),
            indices: Vec::new(),
        }
    }

    pub fn triangle_mesh() -> Self {
        Self::new(GeometryTopology::Triangles)
    }

    pub fn plane(size: Vec2) -> Self {
        PrimitiveShape::Plane {
            size,
            width_segments: 1,
            depth_segments: 1,
        }
        .build_geometry()
    }

    pub fn box_mesh(size: Vec3) -> Self {
        PrimitiveShape::Box {
            size,
            width_segments: 1,
            height_segments: 1,
            depth_segments: 1,
        }
        .build_geometry()
    }

    pub fn uv_sphere(radius: f32, latitude_segments: usize, longitude_segments: usize) -> Self {
        PrimitiveShape::UvSphere {
            radius,
            latitude_segments,
            longitude_segments,
        }
        .build_geometry()
    }

    pub fn cylinder(
        radius: f32,
        height: f32,
        radial_segments: usize,
        height_segments: usize,
    ) -> Self {
        PrimitiveShape::Cylinder {
            radius,
            height,
            radial_segments,
            height_segments,
            cap_segments: 1,
        }
        .build_geometry()
    }

    pub fn cone(radius: f32, height: f32, radial_segments: usize, height_segments: usize) -> Self {
        PrimitiveShape::Cone {
            radius,
            height,
            radial_segments,
            height_segments,
            cap_segments: 1,
        }
        .build_geometry()
    }

    pub fn capsule(
        radius: f32,
        height: f32,
        radial_segments: usize,
        hemisphere_segments: usize,
        body_segments: usize,
    ) -> Self {
        PrimitiveShape::Capsule {
            radius,
            height,
            radial_segments,
            hemisphere_segments,
            body_segments,
        }
        .build_geometry()
    }

    pub fn torus(
        major_radius: f32,
        minor_radius: f32,
        major_segments: usize,
        minor_segments: usize,
    ) -> Self {
        PrimitiveShape::Torus {
            major_radius,
            minor_radius,
            major_segments,
            minor_segments,
        }
        .build_geometry()
    }

    pub fn quad_sphere(radius: f32, resolution: usize) -> Self {
        PrimitiveShape::QuadSphere { radius, resolution }.build_geometry()
    }

    pub fn from_mesh(mesh: &Mesh) -> Self {
        Self::triangle_mesh()
            .with_positions(mesh.vertices.iter().map(|vertex| vertex.position).collect())
            .with_normals(mesh.vertices.iter().map(|vertex| vertex.normal).collect())
            .with_indices(
                mesh.triangles
                    .iter()
                    .flat_map(|triangle| triangle.map(|index| index as u32))
                    .collect(),
            )
    }

    pub fn with_positions(mut self, values: Vec<Vec3>) -> Self {
        self.attributes.insert(
            "position".to_string(),
            GeometryAttribute::new(
                "position",
                AttributeDomain::Vertex,
                AttributeValues::Vec3(values),
            ),
        );
        self
    }

    pub fn with_normals(mut self, values: Vec<Vec3>) -> Self {
        self.attributes.insert(
            "normal".to_string(),
            GeometryAttribute::new(
                "normal",
                AttributeDomain::Vertex,
                AttributeValues::Vec3(values),
            ),
        );
        self
    }

    pub fn with_uvs(mut self, values: Vec<Vec2>) -> Self {
        self.attributes.insert(
            "uv".to_string(),
            GeometryAttribute::new("uv", AttributeDomain::Vertex, AttributeValues::Vec2(values)),
        );
        self
    }

    pub fn with_colors(mut self, values: Vec<ColorRgb>) -> Self {
        self.attributes.insert(
            "color".to_string(),
            GeometryAttribute::new(
                "color",
                AttributeDomain::Vertex,
                AttributeValues::Color(values),
            ),
        );
        self
    }

    pub fn with_indices(mut self, indices: Vec<u32>) -> Self {
        self.indices = indices;
        self
    }

    pub fn set_attribute(&mut self, attribute: GeometryAttribute) -> &mut Self {
        self.attributes.insert(attribute.name.clone(), attribute);
        self
    }

    pub fn vertex_count(&self) -> usize {
        self.positions().map_or(0, <[Vec3]>::len)
    }

    pub fn positions(&self) -> Option<&[Vec3]> {
        match self.attributes.get("position") {
            Some(GeometryAttribute {
                values: AttributeValues::Vec3(values),
                ..
            }) => Some(values),
            _ => None,
        }
    }

    pub fn normals(&self) -> Option<&[Vec3]> {
        match self.attributes.get("normal") {
            Some(GeometryAttribute {
                values: AttributeValues::Vec3(values),
                ..
            }) => Some(values),
            _ => None,
        }
    }

    pub fn uvs(&self) -> Option<&[Vec2]> {
        match self.attributes.get("uv") {
            Some(GeometryAttribute {
                values: AttributeValues::Vec2(values),
                ..
            }) => Some(values),
            _ => None,
        }
    }

    fn set_positions(&mut self, values: Vec<Vec3>) {
        self.attributes.insert(
            "position".to_string(),
            GeometryAttribute::new(
                "position",
                AttributeDomain::Vertex,
                AttributeValues::Vec3(values),
            ),
        );
    }

    fn set_normals(&mut self, values: Vec<Vec3>) {
        self.attributes.insert(
            "normal".to_string(),
            GeometryAttribute::new(
                "normal",
                AttributeDomain::Vertex,
                AttributeValues::Vec3(values),
            ),
        );
    }

    pub fn validate(&self) -> Result<(), GeometryError> {
        let positions = self.positions().ok_or(GeometryError::MissingPositions)?;
        let vertex_count = positions.len();

        for (name, attribute) in &self.attributes {
            if attribute.domain == AttributeDomain::Vertex && attribute.values.len() != vertex_count
            {
                return Err(GeometryError::InvalidAttributeLength {
                    attribute: name.clone(),
                    expected: vertex_count,
                    actual: attribute.values.len(),
                });
            }
        }

        if self.topology == GeometryTopology::Triangles
            && !self.indices.is_empty()
            && self.indices.len() % 3 != 0
        {
            return Err(GeometryError::InvalidTriangleIndexCount(self.indices.len()));
        }

        for index in &self.indices {
            if *index as usize >= vertex_count {
                return Err(GeometryError::InvalidIndex(*index));
            }
        }

        Ok(())
    }

    pub fn compute_vertex_normals(&mut self) -> Result<&mut Self, GeometryError> {
        self.validate()?;
        if self.topology != GeometryTopology::Triangles {
            return Err(GeometryError::UnsupportedTopology(self.topology));
        }

        let positions = self.positions().ok_or(GeometryError::MissingPositions)?;
        let normals = generate_vertex_normals(positions, &self.indices)?;
        self.set_normals(normals);
        Ok(self)
    }

    pub fn transformed(&self, transform: &Transform) -> Result<Self, GeometryError> {
        self.validate()?;
        let mut output = self.clone();
        let positions = self
            .positions()
            .ok_or(GeometryError::MissingPositions)?
            .iter()
            .copied()
            .map(|position| transform.transform_point(position))
            .collect();
        output.set_positions(positions);

        if let Some(normals) = self.normals() {
            let rotation = Mat4::rotation_xyz(transform.rotation_radians);
            output.set_normals(
                normals
                    .iter()
                    .copied()
                    .map(|normal| rotation.transform_vector(normal).normalize())
                    .collect(),
            );
        }

        Ok(output)
    }

    pub fn apply_modifiers(&self, modifiers: &[Modifier]) -> Result<Self, GeometryError> {
        let mut output = self.clone();
        for modifier in modifiers {
            modifier.apply(&mut output)?;
        }
        Ok(output)
    }

    pub fn to_mesh(&self) -> Result<Mesh, GeometryError> {
        self.validate()?;
        if self.topology != GeometryTopology::Triangles {
            return Err(GeometryError::UnsupportedTopology(self.topology));
        }

        let positions = self.positions().ok_or(GeometryError::MissingPositions)?;
        let normals = match self.normals() {
            Some(values) => values.to_vec(),
            None => generate_vertex_normals(positions, &self.indices)?,
        };
        let vertices = positions
            .iter()
            .zip(normals.iter())
            .map(|(position, normal)| Vertex {
                position: *position,
                normal: *normal,
            })
            .collect();

        let triangles = if self.indices.is_empty() {
            (0..positions.len())
                .step_by(3)
                .map(|index| [index, index + 1, index + 2])
                .collect()
        } else {
            self.indices
                .chunks_exact(3)
                .map(|chunk| [chunk[0] as usize, chunk[1] as usize, chunk[2] as usize])
                .collect()
        };

        Ok(Mesh {
            vertices,
            triangles,
        })
    }
}

#[derive(Clone, Debug, PartialEq)]
pub enum Field {
    Constant(f32),
    Linear {
        axis: Vec3,
        start: f32,
        end: f32,
    },
    Radial {
        center: Vec3,
        inner_radius: f32,
        outer_radius: f32,
    },
    Noise {
        frequency: f32,
        offset: Vec3,
    },
    VolumeMask {
        volume: Volume,
        feather: f32,
    },
}

impl Field {
    pub fn sample(&self, point: Vec3) -> f32 {
        match self {
            Self::Constant(value) => value.clamp(0.0, 1.0),
            Self::Linear { axis, start, end } => {
                let axis = axis.normalize();
                let projected = point.dot(axis);
                smoothstep(*start, *end, projected)
            }
            Self::Radial {
                center,
                inner_radius,
                outer_radius,
            } => {
                let distance = point.distance(*center);
                if distance <= *inner_radius {
                    1.0
                } else if distance >= *outer_radius {
                    0.0
                } else {
                    1.0 - smoothstep(*inner_radius, *outer_radius, distance)
                }
            }
            Self::Noise { frequency, offset } => {
                let sample = pseudo_noise(point * *frequency + *offset);
                (sample * 0.5 + 0.5).clamp(0.0, 1.0)
            }
            Self::VolumeMask { volume, feather } => {
                let distance = volume.sample_sdf(point);
                if distance <= 0.0 {
                    1.0
                } else if *feather <= f32::EPSILON {
                    0.0
                } else {
                    (1.0 - distance / *feather).clamp(0.0, 1.0)
                }
            }
        }
    }
}

#[derive(Clone, Debug, PartialEq)]
pub enum Modifier {
    Translate(Vec3),
    Scale(Vec3),
    Inflate {
        amount: f32,
    },
    Twist {
        axis: Vec3,
        angle_radians: f32,
        field: Option<Field>,
    },
    NoiseDisplace {
        amplitude: f32,
        frequency: f32,
        along_normal: bool,
        field: Option<Field>,
    },
    Spherify {
        factor: f32,
    },
}

impl Modifier {
    pub fn apply(&self, geometry: &mut Geometry) -> Result<(), GeometryError> {
        geometry.validate()?;
        let positions = geometry
            .positions()
            .ok_or(GeometryError::MissingPositions)?
            .to_vec();
        let normals = geometry.normals().map(ToOwned::to_owned);
        let mut next_positions = positions.clone();

        match self {
            Self::Translate(offset) => {
                for position in &mut next_positions {
                    *position += *offset;
                }
            }
            Self::Scale(scale) => {
                for position in &mut next_positions {
                    *position = position.component_mul(*scale);
                }
            }
            Self::Inflate { amount } => {
                for (index, position) in next_positions.iter_mut().enumerate() {
                    let normal = normals
                        .as_ref()
                        .and_then(|values| values.get(index))
                        .copied()
                        .unwrap_or_else(|| positions[index].normalize());
                    *position += normal * *amount;
                }
            }
            Self::Twist {
                axis,
                angle_radians,
                field,
            } => {
                let axis = axis.normalize();
                for (index, position) in next_positions.iter_mut().enumerate() {
                    let weight = field
                        .as_ref()
                        .map(|value| value.sample(positions[index]))
                        .unwrap_or(1.0);
                    let distance_along_axis = positions[index].dot(axis);
                    let angle = angle_radians * distance_along_axis * weight;
                    *position = rotate_around_axis(positions[index], axis, angle);
                }
            }
            Self::NoiseDisplace {
                amplitude,
                frequency,
                along_normal,
                field,
            } => {
                for (index, position) in next_positions.iter_mut().enumerate() {
                    let weight = field
                        .as_ref()
                        .map(|value| value.sample(positions[index]))
                        .unwrap_or(1.0);
                    let noise = pseudo_noise(positions[index] * *frequency) * amplitude * weight;
                    let direction = if *along_normal {
                        normals
                            .as_ref()
                            .and_then(|values| values.get(index))
                            .copied()
                            .unwrap_or_else(|| positions[index].normalize())
                    } else {
                        Vec3::new(
                            pseudo_noise(
                                positions[index] * (*frequency * 0.73) + Vec3::new(17.0, 0.0, 0.0),
                            ),
                            pseudo_noise(
                                positions[index] * (*frequency * 0.89) + Vec3::new(0.0, 23.0, 0.0),
                            ),
                            pseudo_noise(
                                positions[index] * (*frequency * 1.11) + Vec3::new(0.0, 0.0, 29.0),
                            ),
                        )
                        .normalize()
                    };
                    *position += direction * noise;
                }
            }
            Self::Spherify { factor } => {
                let average_radius = if positions.is_empty() {
                    0.0
                } else {
                    positions
                        .iter()
                        .map(|position| position.length())
                        .sum::<f32>()
                        / positions.len() as f32
                };
                for (index, position) in next_positions.iter_mut().enumerate() {
                    let target = positions[index].normalize() * average_radius;
                    *position = positions[index].lerp(target, factor.clamp(0.0, 1.0));
                }
            }
        }

        geometry.set_positions(next_positions);
        if normals.is_some() && !matches!(self, Self::Translate(_)) {
            geometry.compute_vertex_normals()?;
        }
        Ok(())
    }
}

#[derive(Clone, Debug, PartialEq)]
pub enum Effector {
    Translate {
        offset: Vec3,
        field: Field,
    },
    Rotate {
        rotation_radians: Vec3,
        field: Field,
    },
    Scale {
        factor: Vec3,
        field: Field,
    },
}

impl Effector {
    pub fn apply(&self, sample_position: Vec3, transform: &mut Transform) {
        let weight = match self {
            Self::Translate { field, .. }
            | Self::Rotate { field, .. }
            | Self::Scale { field, .. } => field.sample(sample_position),
        };

        match self {
            Self::Translate { offset, .. } => {
                transform.translation += *offset * weight;
            }
            Self::Rotate {
                rotation_radians, ..
            } => {
                transform.rotation_radians += *rotation_radians * weight;
            }
            Self::Scale { factor, .. } => {
                let weighted = Vec3::new(
                    1.0 + (factor.x - 1.0) * weight,
                    1.0 + (factor.y - 1.0) * weight,
                    1.0 + (factor.z - 1.0) * weight,
                );
                transform.scale = transform.scale.component_mul(weighted);
            }
        }
    }
}

#[derive(Clone, Debug, PartialEq)]
pub enum SplineType {
    Linear,
    CatmullRom,
}

#[derive(Clone, Debug, PartialEq)]
pub struct Spline {
    pub points: Vec<Vec3>,
    pub spline_type: SplineType,
    pub closed: bool,
}

impl Spline {
    pub fn line(points: Vec<Vec3>) -> Self {
        Self {
            points,
            spline_type: SplineType::Linear,
            closed: false,
        }
    }

    pub fn catmull_rom(points: Vec<Vec3>, closed: bool) -> Self {
        Self {
            points,
            spline_type: SplineType::CatmullRom,
            closed,
        }
    }

    pub fn sample(&self, t: f32) -> Vec3 {
        if self.points.is_empty() {
            return Vec3::ZERO;
        }
        if self.points.len() == 1 {
            return self.points[0];
        }

        let point_count = if self.closed {
            self.points.len()
        } else {
            self.points.len() - 1
        };
        let scaled = t.clamp(0.0, 1.0) * point_count as f32;
        let segment = scaled.floor() as usize;
        let local_t = scaled - segment as f32;

        match self.spline_type {
            SplineType::Linear => {
                let a = self.points[segment.min(self.points.len() - 1)];
                let b = self.points[self.wrap_index(segment + 1)];
                a.lerp(b, local_t)
            }
            SplineType::CatmullRom => {
                let p0 = self.points[self.wrap_index(segment.saturating_sub(1))];
                let p1 = self.points[self.wrap_index(segment)];
                let p2 = self.points[self.wrap_index(segment + 1)];
                let p3 = self.points[self.wrap_index(segment + 2)];
                catmull_rom(p0, p1, p2, p3, local_t)
            }
        }
    }

    pub fn tangent(&self, t: f32) -> Vec3 {
        let delta = 0.001;
        let a = self.sample((t - delta).clamp(0.0, 1.0));
        let b = self.sample((t + delta).clamp(0.0, 1.0));
        (b - a).normalize()
    }

    fn wrap_index(&self, index: usize) -> usize {
        if self.closed {
            index % self.points.len()
        } else {
            index.min(self.points.len() - 1)
        }
    }
}

#[derive(Clone, Debug, PartialEq)]
pub enum Volume {
    Sphere {
        radius: f32,
    },
    Box {
        half_extents: Vec3,
    },
    Capsule {
        radius: f32,
        half_height: f32,
    },
    Union(Vec<Volume>),
    Difference {
        base: Box<Volume>,
        subtract: Box<Volume>,
    },
}

impl Volume {
    pub fn sample_sdf(&self, point: Vec3) -> f32 {
        match self {
            Self::Sphere { radius } => point.length() - radius,
            Self::Box { half_extents } => {
                let delta = abs_vec3(point) - *half_extents;
                let outside = Vec3::new(delta.x.max(0.0), delta.y.max(0.0), delta.z.max(0.0));
                outside.length() + delta.x.max(delta.y.max(delta.z)).min(0.0)
            }
            Self::Capsule {
                radius,
                half_height,
            } => {
                let clamped_y = point.y.clamp(-*half_height, *half_height);
                let closest = Vec3::new(0.0, clamped_y, 0.0);
                (point - closest).length() - radius
            }
            Self::Union(volumes) => volumes
                .iter()
                .map(|volume| volume.sample_sdf(point))
                .fold(f32::INFINITY, f32::min),
            Self::Difference { base, subtract } => {
                base.sample_sdf(point).max(-subtract.sample_sdf(point))
            }
        }
    }
}

#[derive(Clone, Debug, PartialEq)]
pub enum InstancePattern {
    Grid {
        counts: [usize; 3],
        spacing: Vec3,
        centered: bool,
    },
    AlongSpline {
        spline: Spline,
        count: usize,
        align_to_tangent: bool,
    },
    Points {
        points: Vec<Vec3>,
    },
}

#[derive(Clone, Debug, PartialEq)]
pub struct Instancer {
    pub geometry: String,
    pub material: String,
    pub pattern: InstancePattern,
    pub effectors: Vec<Effector>,
}

impl Instancer {
    pub fn grid(
        geometry: impl Into<String>,
        material: impl Into<String>,
        counts: [usize; 3],
        spacing: Vec3,
    ) -> Self {
        Self {
            geometry: geometry.into(),
            material: material.into(),
            pattern: InstancePattern::Grid {
                counts,
                spacing,
                centered: true,
            },
            effectors: Vec::new(),
        }
    }

    pub fn generate_transforms(&self) -> Vec<Transform> {
        let mut transforms = match &self.pattern {
            InstancePattern::Grid {
                counts,
                spacing,
                centered,
            } => generate_grid_transforms(*counts, *spacing, *centered),
            InstancePattern::AlongSpline {
                spline,
                count,
                align_to_tangent,
            } => {
                let count = (*count).max(1);
                (0..count)
                    .map(|index| {
                        let t = if count == 1 {
                            0.0
                        } else {
                            index as f32 / (count - 1) as f32
                        };
                        let position = spline.sample(t);
                        let mut transform = Transform::identity().with_translation(position);
                        if *align_to_tangent {
                            let tangent = spline.tangent(t);
                            transform.rotation_radians.y = tangent.z.atan2(tangent.x);
                        }
                        transform
                    })
                    .collect()
            }
            InstancePattern::Points { points } => points
                .iter()
                .copied()
                .map(|position| Transform::identity().with_translation(position))
                .collect(),
        };

        for transform in &mut transforms {
            let sample_position = transform.translation;
            for effector in &self.effectors {
                effector.apply(sample_position, transform);
            }
        }

        transforms
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum BrushFalloff {
    Constant,
    Linear,
    Smooth,
    Sphere,
}

#[derive(Clone, Debug, PartialEq)]
pub struct Brush {
    pub radius: f32,
    pub strength: f32,
    pub spacing: f32,
    pub falloff: BrushFalloff,
}

impl Brush {
    pub fn sample_weight(&self, distance: f32, pressure: f32) -> f32 {
        if self.radius <= f32::EPSILON {
            return 0.0;
        }

        let normalized = (distance / self.radius).clamp(0.0, 1.0);
        let falloff = match self.falloff {
            BrushFalloff::Constant => 1.0,
            BrushFalloff::Linear => 1.0 - normalized,
            BrushFalloff::Smooth => 1.0 - smoothstep(0.0, 1.0, normalized),
            BrushFalloff::Sphere => (1.0 - normalized * normalized).max(0.0).sqrt(),
        };
        (self.strength * pressure * falloff).clamp(0.0, 1.0)
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct NodeId(pub usize);

#[derive(Clone, Debug, PartialEq)]
pub struct ToolContext {
    pub active_scene: String,
    pub cursor_world: Vec3,
    pub cursor_normal: Vec3,
    pub pressure: f32,
    pub time_seconds: f32,
    pub brush: Brush,
    pub selected_nodes: BTreeSet<NodeId>,
}

impl ToolContext {
    pub fn stroke_weight(&self, point: Vec3) -> f32 {
        self.brush
            .sample_weight(point.distance(self.cursor_world), self.pressure)
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct MeshNode {
    pub geometry: String,
    pub material: String,
    pub modifiers: Vec<Modifier>,
}

#[derive(Clone, Debug, PartialEq)]
pub enum Light {
    Directional(DirectionalLight),
    Point(PointLight),
}

impl Light {
    pub fn directional(direction: Vec3, color: ColorRgb, intensity: f32) -> Self {
        Self::Directional(DirectionalLight {
            direction: direction.normalize(),
            color,
            intensity,
        })
    }

    pub fn point(color: ColorRgb, intensity: f32, range: f32) -> Self {
        Self::Point(PointLight {
            position: Vec3::ZERO,
            color,
            intensity,
            range,
        })
    }
}

#[derive(Clone, Debug, PartialEq)]
pub enum NodeKind {
    Empty,
    Mesh(MeshNode),
    Camera(Camera),
    Light(Light),
    Spline(Spline),
    Volume(Volume),
    Instancer(Instancer),
    Brush(Brush),
    ToolContext(ToolContext),
}

#[derive(Clone, Debug, PartialEq)]
pub struct Node {
    pub id: NodeId,
    pub name: String,
    pub transform: Transform,
    pub kind: NodeKind,
    pub children: Vec<NodeId>,
}

impl Node {
    pub fn new(id: NodeId, name: impl Into<String>, kind: NodeKind) -> Self {
        Self {
            id,
            name: name.into(),
            transform: Transform::identity(),
            kind,
            children: Vec::new(),
        }
    }
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum SceneBuildError {
    MissingNode(NodeId),
    MissingGeometry(String),
    MissingMaterial(String),
    Geometry(GeometryError),
}

impl std::fmt::Display for SceneBuildError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::MissingNode(node) => write!(f, "scene references missing node `{}`", node.0),
            Self::MissingGeometry(name) => write!(f, "scene references missing geometry `{name}`"),
            Self::MissingMaterial(name) => write!(f, "scene references missing material `{name}`"),
            Self::Geometry(error) => write!(f, "{error}"),
        }
    }
}

impl std::error::Error for SceneBuildError {}

impl From<GeometryError> for SceneBuildError {
    fn from(value: GeometryError) -> Self {
        Self::Geometry(value)
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct Scene {
    pub name: String,
    pub viewport_summary: String,
    pub background: BackgroundGradient,
    pub base_camera: Camera,
    pub base_lighting: LightingRig,
    pub geometries: BTreeMap<String, Geometry>,
    pub materials: BTreeMap<String, Material>,
    pub metadata: BTreeMap<String, String>,
    pub nodes: BTreeMap<NodeId, Node>,
    pub root_nodes: Vec<NodeId>,
    next_node_id: usize,
}

impl Scene {
    pub fn new(name: impl Into<String>) -> Self {
        Self {
            name: name.into(),
            viewport_summary: "authoring scene".to_string(),
            background: BackgroundGradient::solid(ColorRgb::new(0.08, 0.10, 0.14)),
            base_camera: Camera::orbit(Vec3::ZERO, 8.0, 2.5),
            base_lighting: LightingRig::studio(),
            geometries: BTreeMap::new(),
            materials: BTreeMap::new(),
            metadata: BTreeMap::new(),
            nodes: BTreeMap::new(),
            root_nodes: Vec::new(),
            next_node_id: 0,
        }
    }

    pub fn add_geometry(&mut self, name: impl Into<String>, geometry: Geometry) -> &mut Self {
        self.geometries.insert(name.into(), geometry);
        self
    }

    pub fn add_primitive_definition(&mut self, definition: &PrimitiveDefinition) -> &mut Self {
        self.geometries
            .insert(definition.id.clone(), definition.build_geometry());
        self.metadata.insert(
            format!("primitive_definition.{}.resource_uri", definition.id),
            definition.resource_uri.clone(),
        );
        self.metadata.insert(
            format!("primitive_definition.{}.display_name", definition.id),
            definition.display_name.clone(),
        );
        self.metadata.insert(
            format!("primitive_definition.{}.subdivision_ready", definition.id),
            definition.subdivision_ready.to_string(),
        );
        self.metadata.insert(
            format!("primitive_definition.{}.authored_intent", definition.id),
            definition.authored_intent.clone(),
        );
        self
    }

    pub fn add_primitive_library(&mut self, library: &PrimitiveLibrary) -> &mut Self {
        self.metadata.insert(
            "primitive_library.resource_document_uri".to_string(),
            library.resource_document_uri.clone(),
        );
        self.metadata.insert(
            "primitive_library.startup_primitive_id".to_string(),
            library.startup_primitive_id.clone(),
        );
        if let Some(startup_definition) = library.definition(&library.startup_primitive_id) {
            self.metadata.insert(
                "primitive_library.startup_primitive_display_name".to_string(),
                startup_definition.display_name.clone(),
            );
        }
        self.metadata.insert(
            "primitive_library.authored_policy".to_string(),
            library.authored_policy.clone(),
        );
        self.metadata.insert(
            "primitive_library.definition_count".to_string(),
            library.definitions.len().to_string(),
        );
        self.metadata.insert(
            "primitive_library.subdivision_ready_count".to_string(),
            library
                .definitions
                .values()
                .filter(|definition| definition.subdivision_ready)
                .count()
                .to_string(),
        );
        self.metadata
            .insert("primitive_library.summary".to_string(), library.summary());
        self.metadata.insert(
            "primitive_library.definition_ids".to_string(),
            library
                .definitions
                .keys()
                .cloned()
                .collect::<Vec<_>>()
                .join(","),
        );
        for definition in library.definitions.values() {
            self.add_primitive_definition(definition);
        }
        self
    }

    pub fn add_material(&mut self, name: impl Into<String>, material: Material) -> &mut Self {
        self.materials.insert(name.into(), material);
        self
    }

    pub fn spawn_node(&mut self, name: impl Into<String>, kind: NodeKind) -> NodeId {
        let id = NodeId(self.next_node_id);
        self.next_node_id += 1;
        let node = Node::new(id, name, kind);
        self.nodes.insert(id, node);
        self.root_nodes.push(id);
        id
    }

    pub fn spawn_mesh(
        &mut self,
        name: impl Into<String>,
        geometry: impl Into<String>,
        material: impl Into<String>,
    ) -> NodeId {
        self.spawn_node(
            name,
            NodeKind::Mesh(MeshNode {
                geometry: geometry.into(),
                material: material.into(),
                modifiers: Vec::new(),
            }),
        )
    }

    pub fn spawn_instancer(&mut self, name: impl Into<String>, instancer: Instancer) -> NodeId {
        self.spawn_node(name, NodeKind::Instancer(instancer))
    }

    pub fn node(&self, id: NodeId) -> Option<&Node> {
        self.nodes.get(&id)
    }

    pub fn node_mut(&mut self, id: NodeId) -> Option<&mut Node> {
        self.nodes.get_mut(&id)
    }

    pub fn attach(&mut self, parent: NodeId, child: NodeId) -> Result<&mut Self, SceneBuildError> {
        if !self.nodes.contains_key(&child) {
            return Err(SceneBuildError::MissingNode(child));
        }
        let parent_node = self
            .nodes
            .get_mut(&parent)
            .ok_or(SceneBuildError::MissingNode(parent))?;
        if !parent_node.children.contains(&child) {
            parent_node.children.push(child);
        }
        self.root_nodes.retain(|candidate| *candidate != child);
        Ok(self)
    }

    pub fn flatten(&self) -> Result<SceneDescription, SceneBuildError> {
        let mut description = SceneDescription {
            name: self.name.clone(),
            viewport_summary: self.viewport_summary.clone(),
            background: self.background.clone(),
            camera: self.base_camera.clone(),
            lighting: self.base_lighting.clone(),
            meshes: BTreeMap::new(),
            materials: self.materials.clone(),
            instances: Vec::new(),
            animations: Vec::new(),
            particle_emitters: Vec::new(),
        };

        for root in &self.root_nodes {
            self.collect_node(*root, &Transform::identity(), &mut description)?;
        }

        Ok(description)
    }

    fn collect_node(
        &self,
        node_id: NodeId,
        parent_world: &Transform,
        description: &mut SceneDescription,
    ) -> Result<(), SceneBuildError> {
        let node = self
            .nodes
            .get(&node_id)
            .ok_or(SceneBuildError::MissingNode(node_id))?;
        let world = parent_world.combine(&node.transform);

        match &node.kind {
            NodeKind::Empty
            | NodeKind::Spline(_)
            | NodeKind::Volume(_)
            | NodeKind::Brush(_)
            | NodeKind::ToolContext(_) => {}
            NodeKind::Camera(camera) => {
                description.camera = camera.clone();
            }
            NodeKind::Light(light) => match light {
                Light::Directional(light) => {
                    let rotation = Mat4::rotation_xyz(world.rotation_radians);
                    let mut copy = light.clone();
                    copy.direction = rotation.transform_vector(copy.direction).normalize();
                    description.lighting.directional_lights.push(copy);
                }
                Light::Point(light) => {
                    let mut copy = light.clone();
                    copy.position = world.transform_point(copy.position);
                    description.lighting.point_lights.push(copy);
                }
            },
            NodeKind::Mesh(mesh_node) => {
                let geometry = self
                    .geometries
                    .get(&mesh_node.geometry)
                    .ok_or_else(|| SceneBuildError::MissingGeometry(mesh_node.geometry.clone()))?;
                if !description.materials.contains_key(&mesh_node.material) {
                    return Err(SceneBuildError::MissingMaterial(mesh_node.material.clone()));
                }

                let final_geometry = geometry.apply_modifiers(&mesh_node.modifiers)?;
                let mesh_name = if mesh_node.modifiers.is_empty() {
                    mesh_node.geometry.clone()
                } else {
                    format!("{}__node_{}", mesh_node.geometry, node.id.0)
                };
                description
                    .meshes
                    .entry(mesh_name.clone())
                    .or_insert(final_geometry.to_mesh()?);
                description.instances.push(SceneInstance {
                    id: node.name.clone(),
                    mesh: mesh_name,
                    material: mesh_node.material.clone(),
                    transform: world.clone(),
                });
            }
            NodeKind::Instancer(instancer) => {
                let geometry = self
                    .geometries
                    .get(&instancer.geometry)
                    .ok_or_else(|| SceneBuildError::MissingGeometry(instancer.geometry.clone()))?;
                if !description.materials.contains_key(&instancer.material) {
                    return Err(SceneBuildError::MissingMaterial(instancer.material.clone()));
                }

                description
                    .meshes
                    .entry(instancer.geometry.clone())
                    .or_insert(geometry.to_mesh()?);

                for (instance_index, local) in
                    instancer.generate_transforms().into_iter().enumerate()
                {
                    description.instances.push(SceneInstance {
                        id: format!("{}_{}", node.name, instance_index),
                        mesh: instancer.geometry.clone(),
                        material: instancer.material.clone(),
                        transform: world.combine(&local),
                    });
                }
            }
        }

        for child in &node.children {
            self.collect_node(*child, &world, description)?;
        }

        Ok(())
    }
}

impl Material {
    pub fn standard(base_color: ColorRgb) -> Self {
        Self {
            base_color,
            specular_color: ColorRgb::WHITE,
            ambient_strength: 0.20,
            diffuse_strength: 1.0,
            specular_strength: 0.28,
            shininess: 18.0,
        }
    }

    pub fn matte(base_color: ColorRgb) -> Self {
        Self {
            specular_strength: 0.06,
            shininess: 6.0,
            ..Self::standard(base_color)
        }
    }

    pub fn glossy(base_color: ColorRgb) -> Self {
        Self {
            specular_strength: 0.52,
            shininess: 36.0,
            ..Self::standard(base_color)
        }
    }
}

impl Camera {
    pub fn orbit(target: Vec3, orbit_radius: f32, orbit_height: f32) -> Self {
        Self {
            target,
            up: Vec3::UP,
            orbit_radius,
            orbit_height,
            orbit_speed_radians_per_second: 0.0,
            fov_y_degrees: 55.0,
            near_plane: 0.1,
            far_plane: 250.0,
        }
    }
}

impl LightingRig {
    pub fn studio() -> Self {
        Self {
            ambient_color: ColorRgb::new(0.72, 0.76, 0.84),
            ambient_intensity: 0.28,
            directional_lights: vec![
                DirectionalLight {
                    direction: Vec3::new(-0.55, -1.0, -0.35).normalize(),
                    color: ColorRgb::WHITE,
                    intensity: 1.10,
                },
                DirectionalLight {
                    direction: Vec3::new(0.45, -0.50, 0.25).normalize(),
                    color: ColorRgb::new(0.34, 0.48, 0.92),
                    intensity: 0.26,
                },
            ],
            point_lights: vec![PointLight {
                position: Vec3::new(2.0, 2.8, 1.8),
                color: ColorRgb::new(1.0, 0.82, 0.68),
                intensity: 0.88,
                range: 10.0,
            }],
        }
    }
}

impl BackgroundGradient {
    pub fn solid(color: ColorRgb) -> Self {
        Self {
            top: color,
            bottom: color,
        }
    }
}

fn generate_vertex_normals(
    positions: &[Vec3],
    indices: &[u32],
) -> Result<Vec<Vec3>, GeometryError> {
    let mut normals = vec![Vec3::ZERO; positions.len()];
    let triangle_source = if indices.is_empty() {
        if positions.len() % 3 != 0 {
            return Err(GeometryError::InvalidTriangleIndexCount(positions.len()));
        }
        (0..positions.len())
            .step_by(3)
            .map(|index| [index as u32, index as u32 + 1, index as u32 + 2])
            .collect::<Vec<_>>()
    } else {
        if indices.len() % 3 != 0 {
            return Err(GeometryError::InvalidTriangleIndexCount(indices.len()));
        }
        indices
            .chunks_exact(3)
            .map(|chunk| [chunk[0], chunk[1], chunk[2]])
            .collect::<Vec<_>>()
    };

    for triangle in triangle_source {
        let a = positions[triangle[0] as usize];
        let b = positions[triangle[1] as usize];
        let c = positions[triangle[2] as usize];
        let normal = (b - a).cross(c - a).normalize();
        normals[triangle[0] as usize] += normal;
        normals[triangle[1] as usize] += normal;
        normals[triangle[2] as usize] += normal;
    }

    for normal in &mut normals {
        *normal = normal.normalize();
    }

    Ok(normals)
}

fn rotate_around_axis(point: Vec3, axis: Vec3, angle: f32) -> Vec3 {
    let axis = axis.normalized_or(Vec3::UP);
    let cos_theta = angle.cos();
    let sin_theta = angle.sin();
    point * cos_theta + axis.cross(point) * sin_theta + axis * axis.dot(point) * (1.0 - cos_theta)
}

fn pseudo_noise(point: Vec3) -> f32 {
    let value = (point.x * 12.9898 + point.y * 78.233 + point.z * 37.719).sin() * 43_758.547;
    value.fract() * 2.0 - 1.0
}

fn smoothstep(edge0: f32, edge1: f32, value: f32) -> f32 {
    if (edge1 - edge0).abs() <= f32::EPSILON {
        return 1.0;
    }
    let t = ((value - edge0) / (edge1 - edge0)).clamp(0.0, 1.0);
    t * t * (3.0 - 2.0 * t)
}

fn catmull_rom(p0: Vec3, p1: Vec3, p2: Vec3, p3: Vec3, t: f32) -> Vec3 {
    let t2 = t * t;
    let t3 = t2 * t;
    (p1 * 2.0
        + (p2 - p0) * t
        + (p0 * 2.0 - p1 * 5.0 + p2 * 4.0 - p3) * t2
        + (p3 - p0 + (p1 - p2) * 3.0) * t3)
        * 0.5
}

fn abs_vec3(value: Vec3) -> Vec3 {
    Vec3::new(value.x.abs(), value.y.abs(), value.z.abs())
}

fn generate_grid_transforms(counts: [usize; 3], spacing: Vec3, centered: bool) -> Vec<Transform> {
    let counts = [counts[0].max(1), counts[1].max(1), counts[2].max(1)];
    let extent = Vec3::new(
        (counts[0] - 1) as f32 * spacing.x,
        (counts[1] - 1) as f32 * spacing.y,
        (counts[2] - 1) as f32 * spacing.z,
    );
    let offset = if centered { extent * -0.5 } else { Vec3::ZERO };
    let mut transforms = Vec::new();

    for z in 0..counts[2] {
        for y in 0..counts[1] {
            for x in 0..counts[0] {
                transforms.push(Transform::identity().with_translation(
                    offset
                        + Vec3::new(
                            x as f32 * spacing.x,
                            y as f32 * spacing.y,
                            z as f32 * spacing.z,
                        ),
                ));
            }
        }
    }

    transforms
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{RenderResolution, SoftwareRenderer};

    #[test]
    fn box_geometry_roundtrips_to_render_mesh() {
        let geometry = Geometry::box_mesh(Vec3::new(2.0, 2.0, 2.0));
        let mesh = geometry.to_mesh().expect("box mesh should convert");
        assert_eq!(mesh.triangles.len(), 12);
        assert_eq!(mesh.vertices.len(), 24);
    }

    #[test]
    fn modifiers_and_effectors_change_authoring_output() {
        let source = Geometry::plane(Vec2::new(2.0, 2.0));
        let deformed = source
            .apply_modifiers(&[
                Modifier::Twist {
                    axis: Vec3::UP,
                    angle_radians: 0.75,
                    field: Some(Field::Linear {
                        axis: Vec3::UP,
                        start: -1.0,
                        end: 1.0,
                    }),
                },
                Modifier::NoiseDisplace {
                    amplitude: 0.15,
                    frequency: 1.2,
                    along_normal: true,
                    field: None,
                },
            ])
            .expect("modifier stack should succeed");

        let instancer = Instancer {
            geometry: "plane".to_string(),
            material: "default".to_string(),
            pattern: InstancePattern::Grid {
                counts: [3, 1, 2],
                spacing: Vec3::new(2.0, 0.0, 2.0),
                centered: true,
            },
            effectors: vec![Effector::Translate {
                offset: Vec3::new(0.0, 1.5, 0.0),
                field: Field::Radial {
                    center: Vec3::ZERO,
                    inner_radius: 0.0,
                    outer_radius: 4.0,
                },
            }],
        };

        assert_eq!(deformed.vertex_count(), source.vertex_count());
        assert_eq!(instancer.generate_transforms().len(), 6);
        assert!(instancer
            .generate_transforms()
            .iter()
            .any(|transform| transform.translation.y > 0.0));
    }

    #[test]
    fn scene_flattens_into_renderable_description() {
        let mut scene = Scene::new("authoring_demo");
        scene
            .add_geometry("box", Geometry::box_mesh(Vec3::new(1.2, 1.2, 1.2)))
            .add_material("hero", Material::glossy(ColorRgb::new(0.25, 0.72, 0.98)))
            .add_material(
                "instanced",
                Material::matte(ColorRgb::new(0.92, 0.62, 0.28)),
            );

        let hero = scene.spawn_mesh("hero", "box", "hero");
        scene
            .node_mut(hero)
            .expect("hero node should exist")
            .transform = Transform::identity().with_translation(Vec3::new(0.0, 0.5, 0.0));
        if let NodeKind::Mesh(mesh) = &mut scene.node_mut(hero).expect("hero node").kind {
            mesh.modifiers.push(Modifier::Spherify { factor: 0.35 });
        }

        let instancer = Instancer::grid("box", "instanced", [4, 1, 4], Vec3::new(1.8, 0.0, 1.8));
        let clones = scene.spawn_instancer("clones", instancer);
        scene
            .node_mut(clones)
            .expect("instancer node should exist")
            .transform = Transform::identity().with_translation(Vec3::new(0.0, -0.6, 0.0));

        let description = scene.flatten().expect("scene should flatten");
        assert!(!description.instances.is_empty());

        let mut renderer = SoftwareRenderer::default();
        let frame = renderer
            .render_scene(&description, 0.0, RenderResolution::new(160, 96))
            .expect("flattened scene should render");

        assert!(frame.stats.triangles_rasterized > 0);
        assert!(frame.rgba.iter().any(|channel| *channel != 0));
    }

    #[test]
    fn zero_axis_rotation_keeps_rotation_math_stable() {
        let point = Vec3::new(0.0, 1.0, 0.0);
        let rotated = rotate_around_axis(point, Vec3::ZERO, 0.5);

        assert!(rotated.x.is_finite());
        assert!(rotated.y.is_finite());
        assert!(rotated.z.is_finite());
    }
}
