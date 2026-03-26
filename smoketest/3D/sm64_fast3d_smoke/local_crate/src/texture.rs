use std::collections::HashMap;

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
