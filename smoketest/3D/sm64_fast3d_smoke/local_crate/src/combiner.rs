use crate::math::Float4;
use crate::model::CombineMode;

#[derive(Clone, Copy, Debug)]
pub struct CombinerState {
    pub mode: CombineMode,
    pub primitive_color: Float4,
    pub env_color: Float4,
}

#[derive(Clone, Copy, Debug)]
pub struct CompiledCombiner {
    mode: CombineMode,
}

impl CompiledCombiner {
    pub fn compile(mode: CombineMode) -> Self {
        Self { mode }
    }

    pub fn shade(
        &self,
        texture_color: Float4,
        vertex_color: Float4,
        primitive_color: Float4,
        env_color: Float4,
    ) -> Float4 {
        match self.mode {
            CombineMode::Texture => texture_color,
            CombineMode::TextureVertex => texture_color * vertex_color,
            CombineMode::TexturePrimitive => texture_color * primitive_color,
            CombineMode::TextureVertexPrimitive => texture_color * vertex_color * primitive_color,
            CombineMode::TextureEnvMix => texture_color * 0.55 + env_color * 0.45,
            CombineMode::Primitive => primitive_color,
            CombineMode::Vertex => vertex_color,
        }
    }
}
