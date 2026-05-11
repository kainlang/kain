use kain_host::bridge;
use kain_host::{Env, FromKainValue, HostResult, HostSession, KainError, ToKainValue, Value};

use crate::{
    emit_kain_prelude, Brush, BrushFalloff, Camera, ColorRgb, Field, Geometry, GeometryTopology,
    Material, Modifier, Vec2, Vec3,
};

pub const KAIN_3D_MODULE_NAME: &str = "zen3d";

pub struct Kain3dSession {
    host: HostSession,
}

impl Kain3dSession {
    pub fn new() -> Self {
        Self {
            host: HostSession::new(),
        }
    }

    pub fn host(&self) -> &HostSession {
        &self.host
    }

    pub fn host_mut(&mut self) -> &mut HostSession {
        &mut self.host
    }

    pub fn prelude_source(&self) -> String {
        emit_kain_prelude()
    }

    pub fn load_source(&mut self, source: &str) -> HostResult<&mut Self> {
        let full_source = format!("{}\n{}", emit_kain_prelude(), source);
        self.host.load_source(&full_source)?;
        install_runtime_natives(self.host.env_mut());
        Ok(self)
    }

    pub fn call<R>(&mut self, function_name: &str, args: Vec<Value>) -> HostResult<R>
    where
        R: FromKainValue,
    {
        self.host.call(function_name, args)
    }
}

impl Default for Kain3dSession {
    fn default() -> Self {
        Self::new()
    }
}

pub fn install_runtime_natives(env: &mut Env) {
    env.register_native_fn("__zen3d_triangle_geometry", native_triangle_geometry);
    env.register_native_fn("__zen3d_standard_material", native_standard_material);
    env.register_native_fn("__zen3d_matte_material", native_matte_material);
    env.register_native_fn("__zen3d_glossy_material", native_glossy_material);
    env.register_native_fn("__zen3d_orbit_camera", native_orbit_camera);
    env.register_native_fn("__zen3d_radial_field", native_radial_field);
    env.register_native_fn("__zen3d_twist", native_twist);
    env.register_native_fn("__zen3d_noise_displace", native_noise_displace);
    env.register_native_fn("__zen3d_brush", native_brush);
}

impl ToKainValue for Vec2 {
    fn to_kain_value(self) -> Value {
        bridge::struct_value(
            "Vec2",
            [("x", self.x.to_kain_value()), ("y", self.y.to_kain_value())],
        )
    }
}

impl FromKainValue for Vec2 {
    fn from_kain_value(value: Value) -> HostResult<Self> {
        let mut fields = bridge::expect_struct(value, "Vec2")?;
        Ok(Self {
            x: bridge::take_struct_field::<f32>(&mut fields, "x")?,
            y: bridge::take_struct_field::<f32>(&mut fields, "y")?,
        })
    }
}

impl ToKainValue for Vec3 {
    fn to_kain_value(self) -> Value {
        bridge::struct_value(
            "Vec3",
            [
                ("x", self.x.to_kain_value()),
                ("y", self.y.to_kain_value()),
                ("z", self.z.to_kain_value()),
            ],
        )
    }
}

impl FromKainValue for Vec3 {
    fn from_kain_value(value: Value) -> HostResult<Self> {
        let mut fields = bridge::expect_struct(value, "Vec3")?;
        Ok(Self {
            x: bridge::take_struct_field::<f32>(&mut fields, "x")?,
            y: bridge::take_struct_field::<f32>(&mut fields, "y")?,
            z: bridge::take_struct_field::<f32>(&mut fields, "z")?,
        })
    }
}

impl ToKainValue for ColorRgb {
    fn to_kain_value(self) -> Value {
        bridge::struct_value(
            "ColorRgb",
            [
                ("r", self.r.to_kain_value()),
                ("g", self.g.to_kain_value()),
                ("b", self.b.to_kain_value()),
            ],
        )
    }
}

impl FromKainValue for ColorRgb {
    fn from_kain_value(value: Value) -> HostResult<Self> {
        let mut fields = bridge::expect_struct(value, "ColorRgb")?;
        Ok(Self {
            r: bridge::take_struct_field::<f32>(&mut fields, "r")?,
            g: bridge::take_struct_field::<f32>(&mut fields, "g")?,
            b: bridge::take_struct_field::<f32>(&mut fields, "b")?,
        })
    }
}

impl ToKainValue for GeometryTopology {
    fn to_kain_value(self) -> Value {
        match self {
            Self::Triangles => bridge::enum_variant_value("GeometryTopology", "Triangles", vec![]),
            Self::Lines => bridge::enum_variant_value("GeometryTopology", "Lines", vec![]),
            Self::Points => bridge::enum_variant_value("GeometryTopology", "Points", vec![]),
        }
    }
}

impl FromKainValue for GeometryTopology {
    fn from_kain_value(value: Value) -> HostResult<Self> {
        let (variant, fields) = bridge::expect_enum(value, "GeometryTopology")?;
        let fields = bridge::expect_variant_len(fields, 0, "GeometryTopology", &variant)?;
        let _ = fields;
        match variant.as_str() {
            "Triangles" => Ok(Self::Triangles),
            "Lines" => Ok(Self::Lines),
            "Points" => Ok(Self::Points),
            _ => Err(KainError::runtime(format!(
                "Unknown GeometryTopology variant `{variant}`"
            ))),
        }
    }
}

impl ToKainValue for Geometry {
    fn to_kain_value(self) -> Value {
        bridge::struct_value(
            "Geometry",
            [
                (
                    "positions",
                    self.positions().unwrap_or(&[]).to_vec().to_kain_value(),
                ),
                (
                    "normals",
                    self.normals().unwrap_or(&[]).to_vec().to_kain_value(),
                ),
                ("uvs", self.uvs().unwrap_or(&[]).to_vec().to_kain_value()),
                ("colors", Vec::<ColorRgb>::new().to_kain_value()),
                (
                    "indices",
                    self.indices
                        .iter()
                        .map(|value| *value as i64)
                        .collect::<Vec<_>>()
                        .to_kain_value(),
                ),
                ("topology", self.topology.to_kain_value()),
            ],
        )
    }
}

impl FromKainValue for Geometry {
    fn from_kain_value(value: Value) -> HostResult<Self> {
        let mut fields = bridge::expect_struct(value, "Geometry")?;
        let positions = bridge::take_struct_field::<Vec<Vec3>>(&mut fields, "positions")?;
        let normals = bridge::take_struct_field::<Vec<Vec3>>(&mut fields, "normals")?;
        let uvs = bridge::take_struct_field::<Vec<Vec2>>(&mut fields, "uvs")?;
        let colors = bridge::take_struct_field::<Vec<ColorRgb>>(&mut fields, "colors")?;
        let indices = bridge::take_struct_field::<Vec<i64>>(&mut fields, "indices")?;
        let topology = bridge::take_struct_field::<GeometryTopology>(&mut fields, "topology")?;
        let mut geometry = Geometry::new(topology)
            .with_positions(positions)
            .with_indices(convert_geometry_indices("__zen3d Geometry", indices)?);
        if !normals.is_empty() {
            geometry = geometry.with_normals(normals);
        }
        if !uvs.is_empty() {
            geometry = geometry.with_uvs(uvs);
        }
        if !colors.is_empty() {
            geometry = geometry.with_colors(colors);
        }
        if geometry.topology == GeometryTopology::Triangles && geometry.normals().is_none() {
            geometry
                .compute_vertex_normals()
                .map_err(|error| KainError::runtime(format!("invalid geometry: {error}")))?;
        }
        geometry
            .validate()
            .map_err(|error| KainError::runtime(format!("invalid geometry: {error}")))?;
        Ok(geometry)
    }
}

impl ToKainValue for Material {
    fn to_kain_value(self) -> Value {
        bridge::struct_value(
            "Material",
            [
                ("base_color", self.base_color.to_kain_value()),
                ("specular_color", self.specular_color.to_kain_value()),
                ("ambient_strength", self.ambient_strength.to_kain_value()),
                ("diffuse_strength", self.diffuse_strength.to_kain_value()),
                ("specular_strength", self.specular_strength.to_kain_value()),
                ("shininess", self.shininess.to_kain_value()),
            ],
        )
    }
}

impl FromKainValue for Material {
    fn from_kain_value(value: Value) -> HostResult<Self> {
        let mut fields = bridge::expect_struct(value, "Material")?;
        Ok(Self {
            base_color: bridge::take_struct_field::<ColorRgb>(&mut fields, "base_color")?,
            specular_color: bridge::take_struct_field::<ColorRgb>(&mut fields, "specular_color")?,
            ambient_strength: bridge::take_struct_field::<f32>(&mut fields, "ambient_strength")?,
            diffuse_strength: bridge::take_struct_field::<f32>(&mut fields, "diffuse_strength")?,
            specular_strength: bridge::take_struct_field::<f32>(&mut fields, "specular_strength")?,
            shininess: bridge::take_struct_field::<f32>(&mut fields, "shininess")?,
        })
    }
}

impl ToKainValue for Camera {
    fn to_kain_value(self) -> Value {
        bridge::struct_value(
            "Camera",
            [
                ("target", self.target.to_kain_value()),
                ("up", self.up.to_kain_value()),
                ("orbit_radius", self.orbit_radius.to_kain_value()),
                ("orbit_height", self.orbit_height.to_kain_value()),
                (
                    "orbit_speed_radians_per_second",
                    self.orbit_speed_radians_per_second.to_kain_value(),
                ),
                ("fov_y_degrees", self.fov_y_degrees.to_kain_value()),
                ("near_plane", self.near_plane.to_kain_value()),
                ("far_plane", self.far_plane.to_kain_value()),
            ],
        )
    }
}

impl FromKainValue for Camera {
    fn from_kain_value(value: Value) -> HostResult<Self> {
        let mut fields = bridge::expect_struct(value, "Camera")?;
        Ok(Self {
            target: bridge::take_struct_field::<Vec3>(&mut fields, "target")?,
            up: bridge::take_struct_field::<Vec3>(&mut fields, "up")?,
            orbit_radius: bridge::take_struct_field::<f32>(&mut fields, "orbit_radius")?,
            orbit_height: bridge::take_struct_field::<f32>(&mut fields, "orbit_height")?,
            orbit_speed_radians_per_second: bridge::take_struct_field::<f32>(
                &mut fields,
                "orbit_speed_radians_per_second",
            )?,
            fov_y_degrees: bridge::take_struct_field::<f32>(&mut fields, "fov_y_degrees")?,
            near_plane: bridge::take_struct_field::<f32>(&mut fields, "near_plane")?,
            far_plane: bridge::take_struct_field::<f32>(&mut fields, "far_plane")?,
        })
    }
}

impl ToKainValue for Field {
    fn to_kain_value(self) -> Value {
        match self {
            Self::Constant(value) => {
                bridge::enum_variant_value("Field", "Constant", vec![value.to_kain_value()])
            }
            Self::Linear { axis, start, end } => bridge::enum_variant_value(
                "Field",
                "Linear",
                vec![
                    axis.to_kain_value(),
                    start.to_kain_value(),
                    end.to_kain_value(),
                ],
            ),
            Self::Radial {
                center,
                inner_radius,
                outer_radius,
            } => bridge::enum_variant_value(
                "Field",
                "Radial",
                vec![
                    center.to_kain_value(),
                    inner_radius.to_kain_value(),
                    outer_radius.to_kain_value(),
                ],
            ),
            Self::Noise { frequency, offset } => bridge::enum_variant_value(
                "Field",
                "Noise",
                vec![frequency.to_kain_value(), offset.to_kain_value()],
            ),
            Self::VolumeMask { .. } => KainError::runtime("VolumeMask bridge is not implemented")
                .to_string()
                .to_kain_value(),
        }
    }
}

impl FromKainValue for Field {
    fn from_kain_value(value: Value) -> HostResult<Self> {
        let (variant, fields) = bridge::expect_enum(value, "Field")?;
        match variant.as_str() {
            "Constant" => {
                let mut fields = bridge::expect_variant_len(fields, 1, "Field", "Constant")?;
                Ok(Self::Constant(f32::from_kain_value(fields.remove(0))?))
            }
            "Linear" => {
                let mut fields = bridge::expect_variant_len(fields, 3, "Field", "Linear")?;
                Ok(Self::Linear {
                    axis: Vec3::from_kain_value(fields.remove(0))?,
                    start: f32::from_kain_value(fields.remove(0))?,
                    end: f32::from_kain_value(fields.remove(0))?,
                })
            }
            "Radial" => {
                let mut fields = bridge::expect_variant_len(fields, 3, "Field", "Radial")?;
                Ok(Self::Radial {
                    center: Vec3::from_kain_value(fields.remove(0))?,
                    inner_radius: f32::from_kain_value(fields.remove(0))?,
                    outer_radius: f32::from_kain_value(fields.remove(0))?,
                })
            }
            "Noise" => {
                let mut fields = bridge::expect_variant_len(fields, 2, "Field", "Noise")?;
                Ok(Self::Noise {
                    frequency: f32::from_kain_value(fields.remove(0))?,
                    offset: Vec3::from_kain_value(fields.remove(0))?,
                })
            }
            _ => Err(KainError::runtime(format!(
                "Unknown Field variant `{variant}`"
            ))),
        }
    }
}

impl ToKainValue for Modifier {
    fn to_kain_value(self) -> Value {
        match self {
            Self::Translate(offset) => {
                bridge::enum_variant_value("Modifier", "Translate", vec![offset.to_kain_value()])
            }
            Self::Scale(factor) => {
                bridge::enum_variant_value("Modifier", "Scale", vec![factor.to_kain_value()])
            }
            Self::Inflate { amount } => {
                bridge::enum_variant_value("Modifier", "Inflate", vec![amount.to_kain_value()])
            }
            Self::Twist {
                axis,
                angle_radians,
                ..
            } => bridge::enum_variant_value(
                "Modifier",
                "Twist",
                vec![axis.to_kain_value(), angle_radians.to_kain_value()],
            ),
            Self::NoiseDisplace {
                amplitude,
                frequency,
                ..
            } => bridge::enum_variant_value(
                "Modifier",
                "NoiseDisplace",
                vec![amplitude.to_kain_value(), frequency.to_kain_value()],
            ),
            Self::Spherify { factor } => {
                bridge::enum_variant_value("Modifier", "Spherify", vec![factor.to_kain_value()])
            }
        }
    }
}

impl FromKainValue for Modifier {
    fn from_kain_value(value: Value) -> HostResult<Self> {
        let (variant, fields) = bridge::expect_enum(value, "Modifier")?;
        match variant.as_str() {
            "Translate" => {
                let mut fields = bridge::expect_variant_len(fields, 1, "Modifier", "Translate")?;
                Ok(Self::Translate(Vec3::from_kain_value(fields.remove(0))?))
            }
            "Scale" => {
                let mut fields = bridge::expect_variant_len(fields, 1, "Modifier", "Scale")?;
                Ok(Self::Scale(Vec3::from_kain_value(fields.remove(0))?))
            }
            "Inflate" => {
                let mut fields = bridge::expect_variant_len(fields, 1, "Modifier", "Inflate")?;
                Ok(Self::Inflate {
                    amount: f32::from_kain_value(fields.remove(0))?,
                })
            }
            "Twist" => {
                let mut fields = bridge::expect_variant_len(fields, 2, "Modifier", "Twist")?;
                Ok(Self::Twist {
                    axis: Vec3::from_kain_value(fields.remove(0))?,
                    angle_radians: f32::from_kain_value(fields.remove(0))?,
                    field: None,
                })
            }
            "NoiseDisplace" => {
                let mut fields =
                    bridge::expect_variant_len(fields, 2, "Modifier", "NoiseDisplace")?;
                Ok(Self::NoiseDisplace {
                    amplitude: f32::from_kain_value(fields.remove(0))?,
                    frequency: f32::from_kain_value(fields.remove(0))?,
                    along_normal: true,
                    field: None,
                })
            }
            "Spherify" => {
                let mut fields = bridge::expect_variant_len(fields, 1, "Modifier", "Spherify")?;
                Ok(Self::Spherify {
                    factor: f32::from_kain_value(fields.remove(0))?,
                })
            }
            _ => Err(KainError::runtime(format!(
                "Unknown Modifier variant `{variant}`"
            ))),
        }
    }
}

impl ToKainValue for BrushFalloff {
    fn to_kain_value(self) -> Value {
        let variant = match self {
            Self::Constant => "Constant",
            Self::Linear => "Linear",
            Self::Smooth => "Smooth",
            Self::Sphere => "Sphere",
        };
        bridge::enum_variant_value("BrushFalloff", variant, vec![])
    }
}

impl FromKainValue for BrushFalloff {
    fn from_kain_value(value: Value) -> HostResult<Self> {
        let (variant, fields) = bridge::expect_enum(value, "BrushFalloff")?;
        let fields = bridge::expect_variant_len(fields, 0, "BrushFalloff", &variant)?;
        let _ = fields;
        match variant.as_str() {
            "Constant" => Ok(Self::Constant),
            "Linear" => Ok(Self::Linear),
            "Smooth" => Ok(Self::Smooth),
            "Sphere" => Ok(Self::Sphere),
            _ => Err(KainError::runtime(format!(
                "Unknown BrushFalloff variant `{variant}`"
            ))),
        }
    }
}

impl ToKainValue for Brush {
    fn to_kain_value(self) -> Value {
        bridge::struct_value(
            "Brush",
            [
                ("radius", self.radius.to_kain_value()),
                ("strength", self.strength.to_kain_value()),
                ("spacing", self.spacing.to_kain_value()),
                ("falloff", self.falloff.to_kain_value()),
            ],
        )
    }
}

impl FromKainValue for Brush {
    fn from_kain_value(value: Value) -> HostResult<Self> {
        let mut fields = bridge::expect_struct(value, "Brush")?;
        Ok(Self {
            radius: bridge::take_struct_field::<f32>(&mut fields, "radius")?,
            strength: bridge::take_struct_field::<f32>(&mut fields, "strength")?,
            spacing: bridge::take_struct_field::<f32>(&mut fields, "spacing")?,
            falloff: bridge::take_struct_field::<BrushFalloff>(&mut fields, "falloff")?,
        })
    }
}

fn native_triangle_geometry(_env: &mut Env, args: Vec<Value>) -> HostResult<Value> {
    match args.as_slice() {
        [positions, normals, uvs, indices] => {
            let positions = Vec::<Vec3>::from_kain_value(positions.clone())?;
            let normals = Vec::<Vec3>::from_kain_value(normals.clone())?;
            let uvs = Vec::<Vec2>::from_kain_value(uvs.clone())?;
            let indices = convert_geometry_indices(
                "__zen3d_triangle_geometry",
                Vec::<i64>::from_kain_value(indices.clone())?,
            )?;

            let mut geometry = Geometry::indexed_triangle_mesh(positions, indices);
            if !normals.is_empty() {
                geometry = geometry.with_normals(normals);
            }
            if !uvs.is_empty() {
                geometry = geometry.with_uvs(uvs);
            }
            if geometry.normals().is_none() {
                geometry.compute_vertex_normals().map_err(|error| {
                    KainError::runtime(format!(
                        "__zen3d_triangle_geometry invalid geometry: {error}"
                    ))
                })?;
            }
            geometry.validate().map_err(|error| {
                KainError::runtime(format!(
                    "__zen3d_triangle_geometry invalid geometry: {error}"
                ))
            })?;
            Ok(geometry.to_kain_value())
        }
        _ => Err(KainError::runtime(
            "__zen3d_triangle_geometry expects (Array<Vec3>, Array<Vec3>, Array<Vec2>, Array<Int>)",
        )),
    }
}

fn native_standard_material(_env: &mut Env, args: Vec<Value>) -> HostResult<Value> {
    let color = expect_single::<ColorRgb>("__zen3d_standard_material", args)?;
    Ok(Material::standard(color).to_kain_value())
}

fn native_matte_material(_env: &mut Env, args: Vec<Value>) -> HostResult<Value> {
    let color = expect_single::<ColorRgb>("__zen3d_matte_material", args)?;
    Ok(Material::matte(color).to_kain_value())
}

fn native_glossy_material(_env: &mut Env, args: Vec<Value>) -> HostResult<Value> {
    let color = expect_single::<ColorRgb>("__zen3d_glossy_material", args)?;
    Ok(Material::glossy(color).to_kain_value())
}

fn native_orbit_camera(_env: &mut Env, args: Vec<Value>) -> HostResult<Value> {
    match args.as_slice() {
        [target, orbit_radius, orbit_height] => Ok(Camera::orbit(
            Vec3::from_kain_value(target.clone())?,
            f32::from_kain_value(orbit_radius.clone())?,
            f32::from_kain_value(orbit_height.clone())?,
        )
        .to_kain_value()),
        _ => Err(KainError::runtime(
            "__zen3d_orbit_camera expects (Vec3, Float, Float)",
        )),
    }
}

fn native_radial_field(_env: &mut Env, args: Vec<Value>) -> HostResult<Value> {
    match args.as_slice() {
        [center, inner_radius, outer_radius] => Ok(Field::Radial {
            center: Vec3::from_kain_value(center.clone())?,
            inner_radius: f32::from_kain_value(inner_radius.clone())?,
            outer_radius: f32::from_kain_value(outer_radius.clone())?,
        }
        .to_kain_value()),
        _ => Err(KainError::runtime(
            "__zen3d_radial_field expects (Vec3, Float, Float)",
        )),
    }
}

fn native_twist(_env: &mut Env, args: Vec<Value>) -> HostResult<Value> {
    match args.as_slice() {
        [axis, angle] => Ok(Modifier::Twist {
            axis: Vec3::from_kain_value(axis.clone())?,
            angle_radians: f32::from_kain_value(angle.clone())?,
            field: None,
        }
        .to_kain_value()),
        _ => Err(KainError::runtime("__zen3d_twist expects (Vec3, Float)")),
    }
}

fn native_noise_displace(_env: &mut Env, args: Vec<Value>) -> HostResult<Value> {
    match args.as_slice() {
        [amplitude, frequency] => Ok(Modifier::NoiseDisplace {
            amplitude: f32::from_kain_value(amplitude.clone())?,
            frequency: f32::from_kain_value(frequency.clone())?,
            along_normal: true,
            field: None,
        }
        .to_kain_value()),
        _ => Err(KainError::runtime(
            "__zen3d_noise_displace expects (Float, Float)",
        )),
    }
}

fn native_brush(_env: &mut Env, args: Vec<Value>) -> HostResult<Value> {
    match args.as_slice() {
        [radius, strength] => Ok(Brush {
            radius: f32::from_kain_value(radius.clone())?,
            strength: f32::from_kain_value(strength.clone())?,
            spacing: 0.15,
            falloff: BrushFalloff::Smooth,
        }
        .to_kain_value()),
        _ => Err(KainError::runtime("__zen3d_brush expects (Float, Float)")),
    }
}

fn expect_single<T>(name: &str, args: Vec<Value>) -> HostResult<T>
where
    T: FromKainValue,
{
    match args.as_slice() {
        [value] => T::from_kain_value(value.clone()),
        _ => Err(KainError::runtime(format!(
            "{name} expects a single argument"
        ))),
    }
}

fn convert_geometry_indices(name: &str, indices: Vec<i64>) -> HostResult<Vec<u32>> {
    indices
        .into_iter()
        .map(|value| {
            u32::try_from(value).map_err(|_| {
                KainError::runtime(format!("{name} received an index outside u32 range"))
            })
        })
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn session_loads_kain_3d_prelude_and_returns_rust_types() {
        let mut session = Kain3dSession::new();
        session
            .load_source(
                r#"
fn build_geometry() -> Geometry:
    return triangle_geometry(
        [
            Vec3 { x: -1.0, y: -1.0, z: 0.0 },
            Vec3 { x: 1.0, y: -1.0, z: 0.0 },
            Vec3 { x: 1.0, y: 1.0, z: 0.0 },
            Vec3 { x: -1.0, y: 1.0, z: 0.0 }
        ],
        [],
        [],
        [0, 1, 2, 0, 2, 3]
    )

fn build_material() -> Material:
    return glossy_material(ColorRgb { r: 0.2, g: 0.6, b: 0.9 })

fn build_modifier() -> Modifier:
    return twist(Vec3 { x: 0.0, y: 1.0, z: 0.0 }, 0.75)
"#,
            )
            .expect("load source with prelude");

        let geometry = session
            .call::<Geometry>("build_geometry", vec![])
            .expect("call build_geometry");
        let material = session
            .call::<Material>("build_material", vec![])
            .expect("call build_material");
        let modifier = session
            .call::<Modifier>("build_modifier", vec![])
            .expect("call build_modifier");

        assert_eq!(geometry.vertex_count(), 4);
        assert_eq!(material.shininess, 36.0);
        assert!(matches!(modifier, Modifier::Twist { .. }));
    }
}
