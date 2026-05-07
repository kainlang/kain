#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Vec2 {
    pub x: f32,
    pub y: f32,
}

impl Vec2 {
    pub const ZERO: Self = Self::new(0.0, 0.0);
    pub const ONE: Self = Self::new(1.0, 1.0);

    pub const fn new(x: f32, y: f32) -> Self {
        Self { x, y }
    }

    pub fn dot(self, other: Self) -> f32 {
        self.x * other.x + self.y * other.y
    }

    pub fn length(self) -> f32 {
        self.dot(self).sqrt()
    }

    pub fn normalize(self) -> Self {
        let length = self.length();
        if length <= f32::EPSILON {
            Self::ZERO
        } else {
            self / length
        }
    }

    pub fn normalized_or(self, fallback: Self) -> Self {
        let normalized = self.normalize();
        if normalized == Self::ZERO {
            fallback
        } else {
            normalized
        }
    }

    pub fn lerp(self, other: Self, t: f32) -> Self {
        self + (other - self) * t
    }
}

impl std::ops::Add for Vec2 {
    type Output = Self;

    fn add(self, rhs: Self) -> Self::Output {
        Self::new(self.x + rhs.x, self.y + rhs.y)
    }
}

impl std::ops::Sub for Vec2 {
    type Output = Self;

    fn sub(self, rhs: Self) -> Self::Output {
        Self::new(self.x - rhs.x, self.y - rhs.y)
    }
}

impl std::ops::Mul<f32> for Vec2 {
    type Output = Self;

    fn mul(self, rhs: f32) -> Self::Output {
        Self::new(self.x * rhs, self.y * rhs)
    }
}

impl std::ops::Div<f32> for Vec2 {
    type Output = Self;

    fn div(self, rhs: f32) -> Self::Output {
        Self::new(self.x / rhs, self.y / rhs)
    }
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Vec3 {
    pub x: f32,
    pub y: f32,
    pub z: f32,
}

impl Vec3 {
    pub const ZERO: Self = Self::new(0.0, 0.0, 0.0);
    pub const UP: Self = Self::new(0.0, 1.0, 0.0);

    pub const fn new(x: f32, y: f32, z: f32) -> Self {
        Self { x, y, z }
    }

    pub fn dot(self, other: Self) -> f32 {
        self.x * other.x + self.y * other.y + self.z * other.z
    }

    pub fn cross(self, other: Self) -> Self {
        Self {
            x: self.y * other.z - self.z * other.y,
            y: self.z * other.x - self.x * other.z,
            z: self.x * other.y - self.y * other.x,
        }
    }

    pub fn length(self) -> f32 {
        self.dot(self).sqrt()
    }

    pub fn normalize(self) -> Self {
        let length = self.length();
        if length <= f32::EPSILON {
            Self::ZERO
        } else {
            self / length
        }
    }

    pub fn normalized_or(self, fallback: Self) -> Self {
        let normalized = self.normalize();
        if normalized == Self::ZERO {
            fallback
        } else {
            normalized
        }
    }

    pub fn component_mul(self, other: Self) -> Self {
        Self::new(self.x * other.x, self.y * other.y, self.z * other.z)
    }

    pub fn distance(self, other: Self) -> f32 {
        (self - other).length()
    }

    pub fn lerp(self, other: Self, t: f32) -> Self {
        self + (other - self) * t
    }
}

impl std::ops::Add for Vec3 {
    type Output = Self;

    fn add(self, rhs: Self) -> Self::Output {
        Self::new(self.x + rhs.x, self.y + rhs.y, self.z + rhs.z)
    }
}

impl std::ops::AddAssign for Vec3 {
    fn add_assign(&mut self, rhs: Self) {
        *self = *self + rhs;
    }
}

impl std::ops::Sub for Vec3 {
    type Output = Self;

    fn sub(self, rhs: Self) -> Self::Output {
        Self::new(self.x - rhs.x, self.y - rhs.y, self.z - rhs.z)
    }
}

impl std::ops::Mul<f32> for Vec3 {
    type Output = Self;

    fn mul(self, rhs: f32) -> Self::Output {
        Self::new(self.x * rhs, self.y * rhs, self.z * rhs)
    }
}

impl std::ops::Div<f32> for Vec3 {
    type Output = Self;

    fn div(self, rhs: f32) -> Self::Output {
        Self::new(self.x / rhs, self.y / rhs, self.z / rhs)
    }
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct ColorRgb {
    pub r: f32,
    pub g: f32,
    pub b: f32,
}

impl ColorRgb {
    pub const BLACK: Self = Self::new(0.0, 0.0, 0.0);
    pub const WHITE: Self = Self::new(1.0, 1.0, 1.0);

    pub const fn new(r: f32, g: f32, b: f32) -> Self {
        Self { r, g, b }
    }

    pub fn to_vec3(self) -> Vec3 {
        Vec3::new(self.r, self.g, self.b)
    }

    pub fn from_vec3(value: Vec3) -> Self {
        Self::new(value.x, value.y, value.z)
    }

    pub fn to_rgba8(self) -> [u8; 4] {
        [
            (self.r.clamp(0.0, 1.0) * 255.0) as u8,
            (self.g.clamp(0.0, 1.0) * 255.0) as u8,
            (self.b.clamp(0.0, 1.0) * 255.0) as u8,
            255,
        ]
    }
}

impl std::ops::Add for ColorRgb {
    type Output = Self;

    fn add(self, rhs: Self) -> Self::Output {
        Self::new(self.r + rhs.r, self.g + rhs.g, self.b + rhs.b)
    }
}

impl std::ops::Mul<f32> for ColorRgb {
    type Output = Self;

    fn mul(self, rhs: f32) -> Self::Output {
        Self::new(self.r * rhs, self.g * rhs, self.b * rhs)
    }
}

impl std::ops::Mul<ColorRgb> for ColorRgb {
    type Output = Self;

    fn mul(self, rhs: ColorRgb) -> Self::Output {
        Self::new(self.r * rhs.r, self.g * rhs.g, self.b * rhs.b)
    }
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Mat4 {
    pub m: [[f32; 4]; 4],
}

impl Mat4 {
    pub fn identity() -> Self {
        Self {
            m: [
                [1.0, 0.0, 0.0, 0.0],
                [0.0, 1.0, 0.0, 0.0],
                [0.0, 0.0, 1.0, 0.0],
                [0.0, 0.0, 0.0, 1.0],
            ],
        }
    }

    pub fn translation(offset: Vec3) -> Self {
        let mut matrix = Self::identity();
        matrix.m[0][3] = offset.x;
        matrix.m[1][3] = offset.y;
        matrix.m[2][3] = offset.z;
        matrix
    }

    pub fn scale(scale: Vec3) -> Self {
        Self {
            m: [
                [scale.x, 0.0, 0.0, 0.0],
                [0.0, scale.y, 0.0, 0.0],
                [0.0, 0.0, scale.z, 0.0],
                [0.0, 0.0, 0.0, 1.0],
            ],
        }
    }

    pub fn rotation_xyz(rotation_radians: Vec3) -> Self {
        let (sx, cx) = rotation_radians.x.sin_cos();
        let (sy, cy) = rotation_radians.y.sin_cos();
        let (sz, cz) = rotation_radians.z.sin_cos();

        let rx = Self {
            m: [
                [1.0, 0.0, 0.0, 0.0],
                [0.0, cx, -sx, 0.0],
                [0.0, sx, cx, 0.0],
                [0.0, 0.0, 0.0, 1.0],
            ],
        };
        let ry = Self {
            m: [
                [cy, 0.0, sy, 0.0],
                [0.0, 1.0, 0.0, 0.0],
                [-sy, 0.0, cy, 0.0],
                [0.0, 0.0, 0.0, 1.0],
            ],
        };
        let rz = Self {
            m: [
                [cz, -sz, 0.0, 0.0],
                [sz, cz, 0.0, 0.0],
                [0.0, 0.0, 1.0, 0.0],
                [0.0, 0.0, 0.0, 1.0],
            ],
        };

        rz.mul_mat4(ry).mul_mat4(rx)
    }

    pub fn perspective(fov_y_radians: f32, aspect_ratio: f32, near: f32, far: f32) -> Self {
        let focal = 1.0 / (fov_y_radians * 0.5).tan();
        let depth = 1.0 / (near - far);

        Self {
            m: [
                [focal / aspect_ratio, 0.0, 0.0, 0.0],
                [0.0, focal, 0.0, 0.0],
                [0.0, 0.0, (far + near) * depth, 2.0 * far * near * depth],
                [0.0, 0.0, -1.0, 0.0],
            ],
        }
    }

    pub fn look_at(eye: Vec3, target: Vec3, up: Vec3) -> Self {
        let forward = (target - eye).normalize();
        let right = forward.cross(up).normalize();
        let corrected_up = right.cross(forward).normalize();

        Self {
            m: [
                [right.x, right.y, right.z, -right.dot(eye)],
                [
                    corrected_up.x,
                    corrected_up.y,
                    corrected_up.z,
                    -corrected_up.dot(eye),
                ],
                [-forward.x, -forward.y, -forward.z, forward.dot(eye)],
                [0.0, 0.0, 0.0, 1.0],
            ],
        }
    }

    pub fn mul_mat4(self, rhs: Self) -> Self {
        let mut output = [[0.0; 4]; 4];
        for row in 0..4 {
            for col in 0..4 {
                output[row][col] = self.m[row][0] * rhs.m[0][col]
                    + self.m[row][1] * rhs.m[1][col]
                    + self.m[row][2] * rhs.m[2][col]
                    + self.m[row][3] * rhs.m[3][col];
            }
        }
        Self { m: output }
    }

    pub fn transform_point(self, point: Vec3) -> [f32; 4] {
        [
            self.m[0][0] * point.x + self.m[0][1] * point.y + self.m[0][2] * point.z + self.m[0][3],
            self.m[1][0] * point.x + self.m[1][1] * point.y + self.m[1][2] * point.z + self.m[1][3],
            self.m[2][0] * point.x + self.m[2][1] * point.y + self.m[2][2] * point.z + self.m[2][3],
            self.m[3][0] * point.x + self.m[3][1] * point.y + self.m[3][2] * point.z + self.m[3][3],
        ]
    }

    pub fn transform_vector(self, vector: Vec3) -> Vec3 {
        Vec3::new(
            self.m[0][0] * vector.x + self.m[0][1] * vector.y + self.m[0][2] * vector.z,
            self.m[1][0] * vector.x + self.m[1][1] * vector.y + self.m[1][2] * vector.z,
            self.m[2][0] * vector.x + self.m[2][1] * vector.y + self.m[2][2] * vector.z,
        )
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct Transform {
    pub translation: Vec3,
    pub rotation_radians: Vec3,
    pub scale: Vec3,
}

impl Transform {
    pub fn identity() -> Self {
        Self {
            translation: Vec3::ZERO,
            rotation_radians: Vec3::ZERO,
            scale: Vec3::new(1.0, 1.0, 1.0),
        }
    }

    pub fn with_translation(mut self, translation: Vec3) -> Self {
        self.translation = translation;
        self
    }

    pub fn with_rotation(mut self, rotation_radians: Vec3) -> Self {
        self.rotation_radians = rotation_radians;
        self
    }

    pub fn with_scale(mut self, scale: Vec3) -> Self {
        self.scale = scale;
        self
    }

    pub fn transform_point(&self, point: Vec3) -> Vec3 {
        let scaled = point.component_mul(self.scale);
        let rotated = Mat4::rotation_xyz(self.rotation_radians).transform_vector(scaled);
        rotated + self.translation
    }

    pub fn transform_vector(&self, vector: Vec3) -> Vec3 {
        Mat4::rotation_xyz(self.rotation_radians).transform_vector(vector.component_mul(self.scale))
    }

    pub fn combine(&self, child: &Self) -> Self {
        let rotated_child_translation = Mat4::rotation_xyz(self.rotation_radians)
            .transform_vector(child.translation.component_mul(self.scale));

        Self {
            translation: self.translation + rotated_child_translation,
            rotation_radians: self.rotation_radians + child.rotation_radians,
            scale: self.scale.component_mul(child.scale),
        }
    }

    pub fn matrix(&self) -> Mat4 {
        Mat4::translation(self.translation)
            .mul_mat4(Mat4::rotation_xyz(self.rotation_radians))
            .mul_mat4(Mat4::scale(self.scale))
    }
}
