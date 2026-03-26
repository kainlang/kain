use crate::combiner::{CombinerState, CompiledCombiner};
use crate::math::{rgba8_from_vec4, Float2, Float4};
use crate::texture::TextureImage;

#[derive(Clone, Debug, Default)]
pub struct RenderStats {
    pub triangles_submitted: usize,
    pub triangles_rasterized: usize,
    pub shaded_pixels: usize,
}

#[derive(Clone, Debug)]
pub struct RenderFrame {
    pub width: usize,
    pub height: usize,
    pub rgba: Vec<u8>,
    pub stats: RenderStats,
}

#[derive(Clone, Copy, Debug)]
pub struct ScreenVertex {
    pub x: f32,
    pub y: f32,
    pub depth: f32,
    pub inv_w: f32,
    pub uv_over_w: Float2,
    pub color_over_w: Float4,
}

pub struct Framebuffer {
    pub width: usize,
    pub height: usize,
    pub rgba: Vec<u8>,
    pub depth: Vec<f32>,
}

impl Framebuffer {
    pub fn new(width: usize, height: usize, clear_color: [u8; 4]) -> Self {
        let mut framebuffer = Self {
            width,
            height,
            rgba: vec![0; width * height * 4],
            depth: vec![f32::INFINITY; width * height],
        };
        framebuffer.clear(clear_color);
        framebuffer
    }

    pub fn clear(&mut self, clear_color: [u8; 4]) {
        for pixel in self.rgba.chunks_exact_mut(4) {
            pixel.copy_from_slice(&clear_color);
        }
        self.depth.fill(f32::INFINITY);
    }

    pub fn draw_triangle(
        &mut self,
        vertices: [ScreenVertex; 3],
        texture: Option<&TextureImage>,
        combiner_state: CombinerState,
        stats: &mut RenderStats,
    ) {
        stats.triangles_submitted += 1;
        let area = edge(vertices[0], vertices[1], vertices[2].x, vertices[2].y);
        if area.abs() <= f32::EPSILON {
            return;
        }

        let min_x = vertices
            .iter()
            .map(|vertex| vertex.x)
            .fold(f32::INFINITY, f32::min)
            .floor()
            .max(0.0) as i32;
        let max_x = vertices
            .iter()
            .map(|vertex| vertex.x)
            .fold(f32::NEG_INFINITY, f32::max)
            .ceil()
            .min((self.width.saturating_sub(1)) as f32) as i32;
        let min_y = vertices
            .iter()
            .map(|vertex| vertex.y)
            .fold(f32::INFINITY, f32::min)
            .floor()
            .max(0.0) as i32;
        let max_y = vertices
            .iter()
            .map(|vertex| vertex.y)
            .fold(f32::NEG_INFINITY, f32::max)
            .ceil()
            .min((self.height.saturating_sub(1)) as f32) as i32;

        let compiled_combiner = CompiledCombiner::compile(combiner_state.mode);
        let winding_sign = area.signum();
        let reciprocal_area = 1.0 / area;
        let primitive_color = combiner_state.primitive_color;
        let env_color = combiner_state.env_color;
        let fallback_texture = Float4::ONE;

        stats.triangles_rasterized += 1;
        for y in min_y..=max_y {
            for x in min_x..=max_x {
                let pixel_center_x = x as f32 + 0.5;
                let pixel_center_y = y as f32 + 0.5;

                let weight0 = edge(vertices[1], vertices[2], pixel_center_x, pixel_center_y);
                let weight1 = edge(vertices[2], vertices[0], pixel_center_x, pixel_center_y);
                let weight2 = edge(vertices[0], vertices[1], pixel_center_x, pixel_center_y);
                if weight0 * winding_sign < 0.0
                    || weight1 * winding_sign < 0.0
                    || weight2 * winding_sign < 0.0
                {
                    continue;
                }

                let bary0 = weight0 * reciprocal_area;
                let bary1 = weight1 * reciprocal_area;
                let bary2 = weight2 * reciprocal_area;

                let inv_w = vertices[0].inv_w * bary0
                    + vertices[1].inv_w * bary1
                    + vertices[2].inv_w * bary2;
                if inv_w <= f32::EPSILON {
                    continue;
                }

                let depth = (vertices[0].depth * vertices[0].inv_w * bary0
                    + vertices[1].depth * vertices[1].inv_w * bary1
                    + vertices[2].depth * vertices[2].inv_w * bary2)
                    / inv_w;
                let pixel_index = y as usize * self.width + x as usize;
                if depth >= self.depth[pixel_index] {
                    continue;
                }

                let uv = (vertices[0].uv_over_w * bary0
                    + vertices[1].uv_over_w * bary1
                    + vertices[2].uv_over_w * bary2)
                    / inv_w;
                let vertex_color = (vertices[0].color_over_w * bary0
                    + vertices[1].color_over_w * bary1
                    + vertices[2].color_over_w * bary2)
                    / inv_w;
                let texture_color = texture
                    .map(|texture| crate::math::vec4_from_rgba8(texture.sample_repeat(uv)))
                    .unwrap_or(fallback_texture);
                let shaded = compiled_combiner.shade(
                    texture_color,
                    vertex_color,
                    primitive_color,
                    env_color,
                );

                self.depth[pixel_index] = depth;
                let output = rgba8_from_vec4(shaded);
                let rgba_index = pixel_index * 4;
                self.rgba[rgba_index..rgba_index + 4].copy_from_slice(&output);
                stats.shaded_pixels += 1;
            }
        }
    }

    pub fn finish(self, stats: RenderStats) -> RenderFrame {
        RenderFrame {
            width: self.width,
            height: self.height,
            rgba: self.rgba,
            stats,
        }
    }
}

fn edge(a: ScreenVertex, b: ScreenVertex, x: f32, y: f32) -> f32 {
    (x - a.x) * (b.y - a.y) - (y - a.y) * (b.x - a.x)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::math::{vec2_from_array, vec4_from_rgba8};
    use crate::model::CombineMode;
    use crate::texture::TextureImage;

    #[test]
    fn textured_triangle_writes_pixels() {
        let texture = TextureImage {
            width: 2,
            height: 2,
            pixels: vec![
                [255, 0, 0, 255],
                [0, 255, 0, 255],
                [0, 0, 255, 255],
                [255, 255, 0, 255],
            ],
        };
        let mut framebuffer = Framebuffer::new(32, 32, [0, 0, 0, 255]);
        let vertices = [
            ScreenVertex {
                x: 4.0,
                y: 4.0,
                depth: 0.3,
                inv_w: 1.0,
                uv_over_w: vec2_from_array([0.0, 0.0]),
                color_over_w: vec4_from_rgba8([255, 255, 255, 255]),
            },
            ScreenVertex {
                x: 28.0,
                y: 6.0,
                depth: 0.3,
                inv_w: 1.0,
                uv_over_w: vec2_from_array([1.0, 0.0]),
                color_over_w: vec4_from_rgba8([255, 255, 255, 255]),
            },
            ScreenVertex {
                x: 16.0,
                y: 28.0,
                depth: 0.3,
                inv_w: 1.0,
                uv_over_w: vec2_from_array([0.5, 1.0]),
                color_over_w: vec4_from_rgba8([255, 255, 255, 255]),
            },
        ];

        let mut stats = RenderStats::default();
        framebuffer.draw_triangle(
            vertices,
            Some(&texture),
            CombinerState {
                mode: CombineMode::Texture,
                primitive_color: vec4_from_rgba8([255, 255, 255, 255]),
                env_color: vec4_from_rgba8([255, 255, 255, 255]),
            },
            &mut stats,
        );

        assert!(stats.shaded_pixels > 0);
        assert!(framebuffer.rgba.iter().any(|value| *value != 0));
    }
}
