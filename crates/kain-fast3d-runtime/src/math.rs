use glam::{Mat4, Vec2, Vec3, Vec4};

pub type Float2 = Vec2;
pub type Float3 = Vec3;
pub type Float4 = Vec4;
pub type Matrix4 = Mat4;

pub fn vec2_from_array(values: [f32; 2]) -> Float2 {
    Float2::new(values[0], values[1])
}

pub fn vec3_from_array(values: [f32; 3]) -> Float3 {
    Float3::new(values[0], values[1], values[2])
}

pub fn vec4_from_rgba8(values: [u8; 4]) -> Float4 {
    Float4::new(
        values[0] as f32 / 255.0,
        values[1] as f32 / 255.0,
        values[2] as f32 / 255.0,
        values[3] as f32 / 255.0,
    )
}

pub fn rgba8_from_vec4(value: Float4) -> [u8; 4] {
    let clamped = value.clamp(Float4::ZERO, Float4::ONE) * 255.0;
    [
        clamped.x.round() as u8,
        clamped.y.round() as u8,
        clamped.z.round() as u8,
        clamped.w.round() as u8,
    ]
}

pub fn matrix_from_rows(rows: [[f32; 4]; 4]) -> Matrix4 {
    Matrix4::from_cols_array_2d(&[
        [rows[0][0], rows[1][0], rows[2][0], rows[3][0]],
        [rows[0][1], rows[1][1], rows[2][1], rows[3][1]],
        [rows[0][2], rows[1][2], rows[2][2], rows[3][2]],
        [rows[0][3], rows[1][3], rows[2][3], rows[3][3]],
    ])
}

pub fn orbit_camera_position(
    target: Float3,
    radius: f32,
    height: f32,
    yaw_radians: f32,
    pitch_radians: f32,
) -> Float3 {
    let horizontal_radius = radius * pitch_radians.cos().max(0.1);
    let camera_height = height + radius * pitch_radians.sin();
    Float3::new(
        target.x + yaw_radians.cos() * horizontal_radius,
        target.y + camera_height,
        target.z + yaw_radians.sin() * horizontal_radius,
    )
}

pub fn transform_from_trs(
    translation: [f32; 3],
    rotation_degrees: [f32; 3],
    scale: [f32; 3],
) -> Matrix4 {
    let translation = vec3_from_array(translation);
    let rotation_radians = vec3_from_array(rotation_degrees).to_radians();
    let rotation = Matrix4::from_euler(
        glam::EulerRot::XYZ,
        rotation_radians.x,
        rotation_radians.y,
        rotation_radians.z,
    );
    Matrix4::from_translation(translation)
        * rotation
        * Matrix4::from_scale(vec3_from_array(scale))
}

pub fn camera_forward(yaw_radians: f32, pitch_radians: f32) -> Float3 {
    Float3::new(
        yaw_radians.cos() * pitch_radians.cos(),
        pitch_radians.sin(),
        yaw_radians.sin() * pitch_radians.cos(),
    )
    .normalize_or_zero()
}
