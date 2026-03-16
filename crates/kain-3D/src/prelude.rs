use kain_reflect::{
    FieldSchema, PrimitiveType, TypeKind, TypeRef, TypeRegistry, TypeSchema, VariantSchema,
    VariantShape,
};

pub fn reflected_type_registry() -> TypeRegistry {
    let mut registry = TypeRegistry::new();

    registry.register_schema(struct_schema(
        "Vec2",
        vec![
            field("x", primitive(PrimitiveType::Float)),
            field("y", primitive(PrimitiveType::Float)),
        ],
    ));
    registry.register_schema(struct_schema(
        "Vec3",
        vec![
            field("x", primitive(PrimitiveType::Float)),
            field("y", primitive(PrimitiveType::Float)),
            field("z", primitive(PrimitiveType::Float)),
        ],
    ));
    registry.register_schema(struct_schema(
        "ColorRgb",
        vec![
            field("r", primitive(PrimitiveType::Float)),
            field("g", primitive(PrimitiveType::Float)),
            field("b", primitive(PrimitiveType::Float)),
        ],
    ));
    registry.register_schema(struct_schema(
        "Transform",
        vec![
            field("translation", named("Vec3")),
            field("rotation_radians", named("Vec3")),
            field("scale", named("Vec3")),
        ],
    ));
    registry.register_schema(enum_schema(
        "GeometryTopology",
        vec![
            unit_variant("Triangles"),
            unit_variant("Lines"),
            unit_variant("Points"),
        ],
    ));
    registry.register_schema(struct_schema(
        "Geometry",
        vec![
            field("positions", array(named("Vec3"))),
            field("normals", array(named("Vec3"))),
            field("uvs", array(named("Vec2"))),
            field("colors", array(named("ColorRgb"))),
            field("indices", array(primitive(PrimitiveType::Int))),
            field("topology", named("GeometryTopology")),
        ],
    ));
    registry.register_schema(struct_schema(
        "Material",
        vec![
            field("base_color", named("ColorRgb")),
            field("specular_color", named("ColorRgb")),
            field("ambient_strength", primitive(PrimitiveType::Float)),
            field("diffuse_strength", primitive(PrimitiveType::Float)),
            field("specular_strength", primitive(PrimitiveType::Float)),
            field("shininess", primitive(PrimitiveType::Float)),
        ],
    ));
    registry.register_schema(struct_schema(
        "Camera",
        vec![
            field("target", named("Vec3")),
            field("up", named("Vec3")),
            field("orbit_radius", primitive(PrimitiveType::Float)),
            field("orbit_height", primitive(PrimitiveType::Float)),
            field("orbit_speed_radians_per_second", primitive(PrimitiveType::Float)),
            field("fov_y_degrees", primitive(PrimitiveType::Float)),
            field("near_plane", primitive(PrimitiveType::Float)),
            field("far_plane", primitive(PrimitiveType::Float)),
        ],
    ));
    registry.register_schema(enum_schema(
        "Field",
        vec![
            tuple_variant("Constant", vec![field("value", primitive(PrimitiveType::Float))]),
            tuple_variant(
                "Linear",
                vec![
                    field("axis", named("Vec3")),
                    field("start", primitive(PrimitiveType::Float)),
                    field("end", primitive(PrimitiveType::Float)),
                ],
            ),
            tuple_variant(
                "Radial",
                vec![
                    field("center", named("Vec3")),
                    field("inner_radius", primitive(PrimitiveType::Float)),
                    field("outer_radius", primitive(PrimitiveType::Float)),
                ],
            ),
            tuple_variant(
                "Noise",
                vec![
                    field("frequency", primitive(PrimitiveType::Float)),
                    field("offset", named("Vec3")),
                ],
            ),
        ],
    ));
    registry.register_schema(enum_schema(
        "Modifier",
        vec![
            tuple_variant("Translate", vec![field("offset", named("Vec3"))]),
            tuple_variant("Scale", vec![field("factor", named("Vec3"))]),
            tuple_variant("Inflate", vec![field("amount", primitive(PrimitiveType::Float))]),
            tuple_variant(
                "Twist",
                vec![
                    field("axis", named("Vec3")),
                    field("angle_radians", primitive(PrimitiveType::Float)),
                ],
            ),
            tuple_variant(
                "NoiseDisplace",
                vec![
                    field("amplitude", primitive(PrimitiveType::Float)),
                    field("frequency", primitive(PrimitiveType::Float)),
                ],
            ),
            tuple_variant("Spherify", vec![field("factor", primitive(PrimitiveType::Float))]),
        ],
    ));
    registry.register_schema(enum_schema(
        "BrushFalloff",
        vec![
            unit_variant("Constant"),
            unit_variant("Linear"),
            unit_variant("Smooth"),
            unit_variant("Sphere"),
        ],
    ));
    registry.register_schema(struct_schema(
        "Brush",
        vec![
            field("radius", primitive(PrimitiveType::Float)),
            field("strength", primitive(PrimitiveType::Float)),
            field("spacing", primitive(PrimitiveType::Float)),
            field("falloff", named("BrushFalloff")),
        ],
    ));

    registry
}

pub fn emit_kain_prelude() -> String {
    let mut prelude = reflected_type_registry().render_kain_prelude();
    prelude.push_str(
        r#"
mod zen3d:
    fn vec2(x: Float, y: Float) -> Vec2:
        return Vec2 { x: x, y: y }

    fn vec3(x: Float, y: Float, z: Float) -> Vec3:
        return Vec3 { x: x, y: y, z: z }

    fn rgb(r: Float, g: Float, b: Float) -> ColorRgb:
        return ColorRgb { r: r, g: g, b: b }

    fn __zen3d_box_geometry(size: Vec3) -> Geometry:
        return Geometry {
            positions: [],
            normals: [],
            uvs: [],
            colors: [],
            indices: [],
            topology: GeometryTopology::Triangles,
        }

    fn __zen3d_plane_geometry(size: Vec2) -> Geometry:
        return Geometry {
            positions: [],
            normals: [],
            uvs: [],
            colors: [],
            indices: [],
            topology: GeometryTopology::Triangles,
        }

    fn __zen3d_uv_sphere(radius: Float, latitude_segments: Int, longitude_segments: Int) -> Geometry:
        let _latitude = latitude_segments
        let _longitude = longitude_segments
        return Geometry {
            positions: [],
            normals: [],
            uvs: [],
            colors: [],
            indices: [],
            topology: GeometryTopology::Triangles,
        }

    fn __zen3d_standard_material(base_color: ColorRgb) -> Material:
        return Material {
            base_color: base_color,
            specular_color: ColorRgb { r: 1.0, g: 1.0, b: 1.0 },
            ambient_strength: 0.2,
            diffuse_strength: 1.0,
            specular_strength: 0.28,
            shininess: 18.0,
        }

    fn __zen3d_matte_material(base_color: ColorRgb) -> Material:
        return Material {
            base_color: base_color,
            specular_color: ColorRgb { r: 1.0, g: 1.0, b: 1.0 },
            ambient_strength: 0.2,
            diffuse_strength: 1.0,
            specular_strength: 0.06,
            shininess: 6.0,
        }

    fn __zen3d_glossy_material(base_color: ColorRgb) -> Material:
        return Material {
            base_color: base_color,
            specular_color: ColorRgb { r: 1.0, g: 1.0, b: 1.0 },
            ambient_strength: 0.2,
            diffuse_strength: 1.0,
            specular_strength: 0.52,
            shininess: 36.0,
        }

    fn __zen3d_orbit_camera(target: Vec3, orbit_radius: Float, orbit_height: Float) -> Camera:
        return Camera {
            target: target,
            up: Vec3 { x: 0.0, y: 1.0, z: 0.0 },
            orbit_radius: orbit_radius,
            orbit_height: orbit_height,
            orbit_speed_radians_per_second: 0.0,
            fov_y_degrees: 55.0,
            near_plane: 0.1,
            far_plane: 250.0,
        }

    fn __zen3d_radial_field(center: Vec3, inner_radius: Float, outer_radius: Float) -> Field:
        return Field::Radial(center, inner_radius, outer_radius)

    fn __zen3d_twist(axis: Vec3, angle_radians: Float) -> Modifier:
        return Modifier::Twist(axis, angle_radians)

    fn __zen3d_noise_displace(amplitude: Float, frequency: Float) -> Modifier:
        return Modifier::NoiseDisplace(amplitude, frequency)

    fn __zen3d_brush(radius: Float, strength: Float) -> Brush:
        return Brush {
            radius: radius,
            strength: strength,
            spacing: 0.15,
            falloff: BrushFalloff::Smooth,
        }

    fn box_geometry(size: Vec3) -> Geometry:
        return __zen3d_box_geometry(size)

    fn plane_geometry(size: Vec2) -> Geometry:
        return __zen3d_plane_geometry(size)

    fn uv_sphere(radius: Float, latitude_segments: Int, longitude_segments: Int) -> Geometry:
        return __zen3d_uv_sphere(radius, latitude_segments, longitude_segments)

    fn standard_material(base_color: ColorRgb) -> Material:
        return __zen3d_standard_material(base_color)

    fn matte_material(base_color: ColorRgb) -> Material:
        return __zen3d_matte_material(base_color)

    fn glossy_material(base_color: ColorRgb) -> Material:
        return __zen3d_glossy_material(base_color)

    fn orbit_camera(target: Vec3, orbit_radius: Float, orbit_height: Float) -> Camera:
        return __zen3d_orbit_camera(target, orbit_radius, orbit_height)

    fn radial_field(center: Vec3, inner_radius: Float, outer_radius: Float) -> Field:
        return __zen3d_radial_field(center, inner_radius, outer_radius)

    fn twist(axis: Vec3, angle_radians: Float) -> Modifier:
        return __zen3d_twist(axis, angle_radians)

    fn noise_displace(amplitude: Float, frequency: Float) -> Modifier:
        return __zen3d_noise_displace(amplitude, frequency)

    fn brush(radius: Float, strength: Float) -> Brush:
        return __zen3d_brush(radius, strength)

use zen3d::*
"#,
    );
    prelude
}

fn primitive(value: PrimitiveType) -> TypeRef {
    TypeRef::Primitive(value)
}

fn named(value: &str) -> TypeRef {
    TypeRef::Named(value.to_string())
}

fn array(value: TypeRef) -> TypeRef {
    TypeRef::Array(Box::new(value))
}

fn field(name: &str, ty: TypeRef) -> FieldSchema {
    FieldSchema::new(name, ty)
}

fn struct_schema(name: &str, fields: Vec<FieldSchema>) -> TypeSchema {
    TypeSchema::new(name, name, TypeKind::Struct { fields })
}

fn enum_schema(name: &str, variants: Vec<VariantSchema>) -> TypeSchema {
    TypeSchema::new(name, name, TypeKind::Enum { variants })
}

fn unit_variant(name: &str) -> VariantSchema {
    VariantSchema::new(name, VariantShape::Unit, Vec::new())
}

fn tuple_variant(name: &str, fields: Vec<FieldSchema>) -> VariantSchema {
    VariantSchema::new(name, VariantShape::Tuple, fields)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn emitted_prelude_contains_core_3d_types_and_runtime_wrappers() {
        let prelude = emit_kain_prelude();
        assert!(prelude.contains("struct Vec3"));
        assert!(prelude.contains("struct Geometry"));
        assert!(prelude.contains("mod zen3d:"));
        assert!(prelude.contains("fn __zen3d_box_geometry"));
        assert!(prelude.contains("use zen3d::*"));
    }
}
