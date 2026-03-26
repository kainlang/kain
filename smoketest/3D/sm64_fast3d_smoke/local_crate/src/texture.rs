use std::collections::HashMap;

use font8x8::UnicodeFonts;

use crate::math::{rgba8_from_vec4, vec4_from_rgba8, Float4};
use crate::model::{TextureDefinition, TextureSource};

#[derive(Clone, Debug)]
pub struct TextureImage {
    pub width: u32,
    pub height: u32,
    pub pixels: Vec<[u8; 4]>,
}

impl TextureImage {
    pub fn sample_repeat(&self, uv: glam::Vec2) -> [u8; 4] {
        let wrap = |value: f32| value.rem_euclid(1.0);
        let u = wrap(uv.x);
        let v = wrap(uv.y);
        let x = ((u * self.width as f32).floor() as u32).min(self.width.saturating_sub(1));
        let y = ((v * self.height as f32).floor() as u32).min(self.height.saturating_sub(1));
        self.pixels[(y * self.width + x) as usize]
    }
}

pub fn build_texture_catalog(
    textures: &[TextureDefinition],
) -> Result<HashMap<String, TextureImage>, String> {
    let mut catalog = HashMap::new();
    for texture in textures {
        let image = match &texture.source {
            TextureSource::Checkerboard {
                width,
                height,
                cell_size,
                color_a,
                color_b,
            } => TextureImage {
                width: *width,
                height: *height,
                pixels: build_checkerboard(*width, *height, *cell_size, *color_a, *color_b),
            },
            TextureSource::Stripes {
                width,
                height,
                stripe_height,
                color_a,
                color_b,
            } => TextureImage {
                width: *width,
                height: *height,
                pixels: build_stripes(*width, *height, *stripe_height, *color_a, *color_b),
            },
            TextureSource::GeneratedSm64TitleCard { width, height } => TextureImage {
                width: *width,
                height: *height,
                pixels: build_sm64_title_card(*width, *height),
            },
            TextureSource::GeneratedMarioEyesFront { width, height } => TextureImage {
                width: *width,
                height: *height,
                pixels: build_mario_eyes_front(*width, *height),
            },
            TextureSource::GeneratedMarioMustache { width, height } => TextureImage {
                width: *width,
                height: *height,
                pixels: build_mario_mustache(*width, *height),
            },
            TextureSource::GeneratedMarioSideburn { width, height } => TextureImage {
                width: *width,
                height: *height,
                pixels: build_mario_sideburn(*width, *height),
            },
        };
        if catalog.insert(texture.id.clone(), image).is_some() {
            return Err(format!("duplicate texture id `{}`", texture.id));
        }
    }
    Ok(catalog)
}

fn build_checkerboard(
    width: u32,
    height: u32,
    cell_size: u32,
    color_a: [u8; 4],
    color_b: [u8; 4],
) -> Vec<[u8; 4]> {
    let cell_size = cell_size.max(1);
    let mut pixels = Vec::with_capacity((width * height) as usize);
    for y in 0..height {
        for x in 0..width {
            let cell_x = x / cell_size;
            let cell_y = y / cell_size;
            let selected = if (cell_x + cell_y) % 2 == 0 {
                color_a
            } else {
                color_b
            };
            pixels.push(selected);
        }
    }
    pixels
}

fn build_stripes(
    width: u32,
    height: u32,
    stripe_height: u32,
    color_a: [u8; 4],
    color_b: [u8; 4],
) -> Vec<[u8; 4]> {
    let stripe_height = stripe_height.max(1);
    let mut pixels = Vec::with_capacity((width * height) as usize);
    let low = vec4_from_rgba8(color_a);
    let high = vec4_from_rgba8(color_b);
    for y in 0..height {
        let stripe_index = y / stripe_height;
        let blend = (y % stripe_height) as f32 / stripe_height as f32;
        for _x in 0..width {
            let base = if stripe_index % 2 == 0 { low } else { high };
            let peak = if stripe_index % 2 == 0 { high } else { low };
            let color: Float4 = base.lerp(peak, blend * 0.6);
            pixels.push(rgba8_from_vec4(color));
        }
    }
    pixels
}

fn build_sm64_title_card(width: u32, height: u32) -> Vec<[u8; 4]> {
    let mut pixels = vec![[0, 0, 0, 255]; (width * height) as usize];
    for y in 0..height {
        let blend = y as f32 / height.max(1) as f32;
        let top = glam::Vec4::new(20.0, 60.0, 140.0, 255.0);
        let bottom = glam::Vec4::new(4.0, 10.0, 34.0, 255.0);
        let sky = top.lerp(bottom, blend) / 255.0;
        for x in 0..width {
            let mut color = rgba8_from_vec4(sky);
            if ((x * 17 + y * 13) % 97) == 0 && y < height / 2 {
                color = [255, 248, 210, 255];
            }
            pixels[(y * width + x) as usize] = color;
        }
    }

    draw_scaled_text(&mut pixels, width, height, 22, 36, 5, "SUPER", [255, 214, 72, 255]);
    draw_scaled_text(&mut pixels, width, height, 58, 108, 6, "MARIO", [234, 52, 42, 255]);
    draw_scaled_text(&mut pixels, width, height, 118, 176, 6, "64", [70, 126, 255, 255]);
    draw_scaled_text(
        &mut pixels,
        width,
        height,
        126,
        340,
        3,
        "PRESS START",
        [252, 252, 252, 255],
    );
    pixels
}

fn build_mario_eyes_front(width: u32, height: u32) -> Vec<[u8; 4]> {
    let mut pixels = vec![[0, 0, 0, 0]; (width * height) as usize];
    fill_ellipse(
        &mut pixels,
        width,
        height,
        width as f32 * 0.3,
        height as f32 * 0.52,
        width as f32 * 0.18,
        height as f32 * 0.2,
        [255, 255, 255, 255],
    );
    fill_ellipse(
        &mut pixels,
        width,
        height,
        width as f32 * 0.7,
        height as f32 * 0.52,
        width as f32 * 0.18,
        height as f32 * 0.2,
        [255, 255, 255, 255],
    );
    fill_ellipse(
        &mut pixels,
        width,
        height,
        width as f32 * 0.3,
        height as f32 * 0.56,
        width as f32 * 0.08,
        height as f32 * 0.1,
        [32, 118, 255, 255],
    );
    fill_ellipse(
        &mut pixels,
        width,
        height,
        width as f32 * 0.7,
        height as f32 * 0.56,
        width as f32 * 0.08,
        height as f32 * 0.1,
        [32, 118, 255, 255],
    );
    fill_ellipse(
        &mut pixels,
        width,
        height,
        width as f32 * 0.3,
        height as f32 * 0.56,
        width as f32 * 0.04,
        height as f32 * 0.06,
        [8, 8, 8, 255],
    );
    fill_ellipse(
        &mut pixels,
        width,
        height,
        width as f32 * 0.7,
        height as f32 * 0.56,
        width as f32 * 0.04,
        height as f32 * 0.06,
        [8, 8, 8, 255],
    );
    pixels
}

fn build_mario_mustache(width: u32, height: u32) -> Vec<[u8; 4]> {
    let mut pixels = vec![[0, 0, 0, 0]; (width * height) as usize];
    for y in 0..height {
        for x in 0..width {
            let xf = x as f32 / width.max(1) as f32;
            let yf = y as f32 / height.max(1) as f32;
            let left = xf > 0.1 && xf < 0.46 && yf > 0.36 + (0.46 - xf) * 0.35 && yf < 0.72;
            let right = xf > 0.54 && xf < 0.9 && yf > 0.36 + (xf - 0.54) * 0.35 && yf < 0.72;
            if left || right {
                pixels[(y * width + x) as usize] = [26, 18, 12, 255];
            }
        }
    }
    pixels
}

fn build_mario_sideburn(width: u32, height: u32) -> Vec<[u8; 4]> {
    let mut pixels = vec![[0, 0, 0, 0]; (width * height) as usize];
    for y in 0..height {
        for x in 0..width {
            let left = x < width / 4 && y > height / 6 && y < height * 5 / 6;
            let right = x > width * 3 / 4 && y > height / 6 && y < height * 5 / 6;
            if left || right {
                pixels[(y * width + x) as usize] = [90, 28, 8, 255];
            }
        }
    }
    pixels
}

fn draw_scaled_text(
    pixels: &mut [[u8; 4]],
    width: u32,
    height: u32,
    start_x: u32,
    start_y: u32,
    scale: u32,
    text: &str,
    color: [u8; 4],
) {
    let scale = scale.max(1);
    let mut cursor_x = start_x;
    for character in text.chars() {
        if character == ' ' {
            cursor_x += 6 * scale;
            continue;
        }
        if let Some(glyph) = font8x8::BASIC_FONTS.get(character) {
            for (row_index, row_bits) in glyph.iter().enumerate() {
                for column_index in 0..8 {
                    if (row_bits >> column_index) & 1 == 0 {
                        continue;
                    }
                    for scale_y in 0..scale {
                        for scale_x in 0..scale {
                            let x = cursor_x + (column_index as u32 * scale) + scale_x;
                            let y = start_y + (row_index as u32 * scale) + scale_y;
                            if x < width && y < height {
                                pixels[(y * width + x) as usize] = color;
                            }
                        }
                    }
                }
            }
        }
        cursor_x += 9 * scale;
    }
}

fn fill_ellipse(
    pixels: &mut [[u8; 4]],
    width: u32,
    height: u32,
    center_x: f32,
    center_y: f32,
    radius_x: f32,
    radius_y: f32,
    color: [u8; 4],
) {
    for y in 0..height {
        for x in 0..width {
            let dx = (x as f32 + 0.5 - center_x) / radius_x.max(f32::EPSILON);
            let dy = (y as f32 + 0.5 - center_y) / radius_y.max(f32::EPSILON);
            if dx * dx + dy * dy <= 1.0 {
                pixels[(y * width + x) as usize] = color;
            }
        }
    }
}
