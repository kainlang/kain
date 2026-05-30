#![allow(dead_code)]
#![allow(unused_variables)]

pub mod kain_gpu_generated {
    #[derive(Debug, Clone, Copy, PartialEq, Eq)]
    pub enum ShaderStage {
        Vertex,
        Fragment,
        Compute,
        Surface,
    }

    #[derive(Debug, Clone, Copy, PartialEq, Eq)]
    pub enum BindingKind {
        StorageBuffer,
        Sampler2D,
        Uniform,
        LocalSize,
        SpecializationConstant,
    }

    #[derive(Debug, Clone, Copy)]
    pub struct BindingDesc {
        pub name: &'static str,
        pub binding: u32,
        pub descriptor_set: u32,
        pub ty: &'static str,
        pub kind: BindingKind,
    }

    #[derive(Debug, Clone, Copy)]
    pub struct BindingLayoutEntry {
        pub binding: u32,
        pub descriptor_set: u32,
        pub kind: BindingKind,
        pub ty: &'static str,
    }

    #[derive(Debug, Clone, Copy)]
    pub struct DispatchSize {
        pub x: u32,
        pub y: u32,
        pub z: u32,
    }

    #[derive(Debug, Clone)]
    pub struct BuiltinInputParam {
        pub name: &'static str,
        pub ty: &'static str,
    }

    #[derive(Debug, Clone)]
    pub struct UniformParam {
        pub ty: &'static str,
    }

    #[derive(Debug, Clone)]
    pub struct StorageBufferParam {
        pub ty: &'static str,
        pub read_only: bool,
    }

    #[derive(Debug, Clone)]
    pub struct Sampler2DParam {
        pub ty: &'static str,
    }

    #[derive(Debug, Clone)]
    pub struct LocalSizeParam {
        pub axis: &'static str,
        pub default_value: u32,
    }

    #[derive(Debug, Clone)]
    pub struct SpecializationConstantParam {
        pub ty: &'static str,
    }

    #[derive(Debug, Clone)]
    pub struct DispatchCall<'a, TParams> {
        pub entry_point: &'static str,
        pub stage: ShaderStage,
        pub size: DispatchSize,
        pub params: &'a TParams,
    }

    #[derive(Debug, Clone, Copy)]
    pub struct ShaderDesc {
        pub name: &'static str,
        pub stage: ShaderStage,
        pub entry_point: &'static str,
        pub output_type: &'static str,
        pub bindings: &'static [BindingDesc],
    }

    pub mod semanticfusedscoretopk {
        use super::{
            BindingDesc, BindingKind, BindingLayoutEntry, BuiltinInputParam, DispatchCall,
            DispatchSize, LocalSizeParam, Sampler2DParam, ShaderDesc, ShaderStage,
            SpecializationConstantParam, StorageBufferParam, UniformParam,
        };

        #[derive(Debug, Clone)]
        pub struct Params {
            pub id: BuiltinInputParam,
            pub query_embed: StorageBufferParam,
            pub index_matrix: StorageBufferParam,
            pub index_weights: StorageBufferParam,
            pub chunk_bias: StorageBufferParam,
            pub block_topk_indices: StorageBufferParam,
            pub block_topk_scores: StorageBufferParam,
            pub warp_scratch_scores: StorageBufferParam,
            pub warp_scratch_indices: StorageBufferParam,
            pub dim: UniformParam,
            pub num_chunks: UniformParam,
            pub top_k: UniformParam,
            pub chunks_per_block: UniformParam,
            pub min_score: UniformParam,
        }

        impl Default for Params {
            fn default() -> Self {
                Self {
                    id: BuiltinInputParam { name: "id", ty: "UVec3" },
                    query_embed: StorageBufferParam { ty: "StorageBuffer<u8>", read_only: false },
                    index_matrix: StorageBufferParam { ty: "StorageBuffer<u8>", read_only: false },
                    index_weights: StorageBufferParam { ty: "StorageBuffer<UInt>", read_only: false },
                    chunk_bias: StorageBufferParam { ty: "StorageBuffer<UInt>", read_only: false },
                    block_topk_indices: StorageBufferParam { ty: "StorageBuffer<UInt>", read_only: false },
                    block_topk_scores: StorageBufferParam { ty: "StorageBuffer<UInt>", read_only: false },
                    warp_scratch_scores: StorageBufferParam { ty: "StorageBuffer<UInt>", read_only: false },
                    warp_scratch_indices: StorageBufferParam { ty: "StorageBuffer<UInt>", read_only: false },
                    dim: UniformParam { ty: "UInt" },
                    num_chunks: UniformParam { ty: "UInt" },
                    top_k: UniformParam { ty: "UInt" },
                    chunks_per_block: UniformParam { ty: "UInt" },
                    min_score: UniformParam { ty: "UInt" },
                }
            }
        }

        pub const BINDINGS: &[BindingDesc] = &[
            BindingDesc { name: "query_embed", binding: 0, descriptor_set: 0, ty: "StorageBuffer<u8>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "index_matrix", binding: 1, descriptor_set: 0, ty: "StorageBuffer<u8>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "index_weights", binding: 2, descriptor_set: 0, ty: "StorageBuffer<UInt>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "chunk_bias", binding: 3, descriptor_set: 0, ty: "StorageBuffer<UInt>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "block_topk_indices", binding: 4, descriptor_set: 0, ty: "StorageBuffer<UInt>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "block_topk_scores", binding: 5, descriptor_set: 0, ty: "StorageBuffer<UInt>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "warp_scratch_scores", binding: 6, descriptor_set: 0, ty: "StorageBuffer<UInt>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "warp_scratch_indices", binding: 7, descriptor_set: 0, ty: "StorageBuffer<UInt>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "dim", binding: 8, descriptor_set: 0, ty: "UInt", kind: BindingKind::Uniform, },
            BindingDesc { name: "num_chunks", binding: 9, descriptor_set: 0, ty: "UInt", kind: BindingKind::Uniform, },
            BindingDesc { name: "top_k", binding: 10, descriptor_set: 0, ty: "UInt", kind: BindingKind::Uniform, },
            BindingDesc { name: "chunks_per_block", binding: 11, descriptor_set: 0, ty: "UInt", kind: BindingKind::Uniform, },
            BindingDesc { name: "min_score", binding: 12, descriptor_set: 0, ty: "UInt", kind: BindingKind::Uniform, },
        ];

        pub const SHADER: ShaderDesc = ShaderDesc { name: "SemanticFusedScoreTopK", stage: ShaderStage::Compute, entry_point: "SemanticFusedScoreTopK", output_type: "Void", bindings: BINDINGS, };

        pub fn descriptor() -> &'static ShaderDesc {
            &SHADER
        }

        pub fn descriptor_layout() -> Vec<BindingLayoutEntry> {
            BINDINGS
                .iter()
                .map(|binding| BindingLayoutEntry {
                    binding: binding.binding,
                    descriptor_set: binding.descriptor_set,
                    kind: binding.kind,
                    ty: binding.ty,
                })
                .collect()
        }

        pub fn dispatch<'a>(params: &'a Params, x: u32, y: u32, z: u32) -> DispatchCall<'a, Params> {
            DispatchCall {
                entry_point: "SemanticFusedScoreTopK",
                stage: ShaderStage::Compute,
                size: DispatchSize { x, y, z },
                params,
            }
        }
    }

    pub mod semanticbitpackdotproduct {
        use super::{
            BindingDesc, BindingKind, BindingLayoutEntry, BuiltinInputParam, DispatchCall,
            DispatchSize, LocalSizeParam, Sampler2DParam, ShaderDesc, ShaderStage,
            SpecializationConstantParam, StorageBufferParam, UniformParam,
        };

        #[derive(Debug, Clone)]
        pub struct Params {
            pub id: BuiltinInputParam,
            pub query_embed: StorageBufferParam,
            pub index_matrix: StorageBufferParam,
            pub index_weights: StorageBufferParam,
            pub chunk_bias: StorageBufferParam,
            pub scores: StorageBufferParam,
            pub dim: UniformParam,
            pub num_chunks: UniformParam,
        }

        impl Default for Params {
            fn default() -> Self {
                Self {
                    id: BuiltinInputParam { name: "id", ty: "UVec3" },
                    query_embed: StorageBufferParam { ty: "StorageBuffer<u8>", read_only: false },
                    index_matrix: StorageBufferParam { ty: "StorageBuffer<u8>", read_only: false },
                    index_weights: StorageBufferParam { ty: "StorageBuffer<UInt>", read_only: false },
                    chunk_bias: StorageBufferParam { ty: "StorageBuffer<UInt>", read_only: false },
                    scores: StorageBufferParam { ty: "StorageBuffer<UInt>", read_only: false },
                    dim: UniformParam { ty: "UInt" },
                    num_chunks: UniformParam { ty: "UInt" },
                }
            }
        }

        pub const BINDINGS: &[BindingDesc] = &[
            BindingDesc { name: "query_embed", binding: 0, descriptor_set: 0, ty: "StorageBuffer<u8>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "index_matrix", binding: 1, descriptor_set: 0, ty: "StorageBuffer<u8>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "index_weights", binding: 2, descriptor_set: 0, ty: "StorageBuffer<UInt>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "chunk_bias", binding: 3, descriptor_set: 0, ty: "StorageBuffer<UInt>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "scores", binding: 4, descriptor_set: 0, ty: "StorageBuffer<UInt>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "dim", binding: 5, descriptor_set: 0, ty: "UInt", kind: BindingKind::Uniform, },
            BindingDesc { name: "num_chunks", binding: 6, descriptor_set: 0, ty: "UInt", kind: BindingKind::Uniform, },
        ];

        pub const SHADER: ShaderDesc = ShaderDesc { name: "SemanticBitpackDotProduct", stage: ShaderStage::Compute, entry_point: "SemanticBitpackDotProduct", output_type: "Void", bindings: BINDINGS, };

        pub fn descriptor() -> &'static ShaderDesc {
            &SHADER
        }

        pub fn descriptor_layout() -> Vec<BindingLayoutEntry> {
            BINDINGS
                .iter()
                .map(|binding| BindingLayoutEntry {
                    binding: binding.binding,
                    descriptor_set: binding.descriptor_set,
                    kind: binding.kind,
                    ty: binding.ty,
                })
                .collect()
        }

        pub fn dispatch<'a>(params: &'a Params, x: u32, y: u32, z: u32) -> DispatchCall<'a, Params> {
            DispatchCall {
                entry_point: "SemanticBitpackDotProduct",
                stage: ShaderStage::Compute,
                size: DispatchSize { x, y, z },
                params,
            }
        }
    }

    pub mod semanticwarptopk {
        use super::{
            BindingDesc, BindingKind, BindingLayoutEntry, BuiltinInputParam, DispatchCall,
            DispatchSize, LocalSizeParam, Sampler2DParam, ShaderDesc, ShaderStage,
            SpecializationConstantParam, StorageBufferParam, UniformParam,
        };

        #[derive(Debug, Clone)]
        pub struct Params {
            pub id: BuiltinInputParam,
            pub scores: StorageBufferParam,
            pub top_indices: StorageBufferParam,
            pub top_scores: StorageBufferParam,
            pub num_chunks: UniformParam,
            pub top_k: UniformParam,
            pub min_score: UniformParam,
            pub taken_mask: StorageBufferParam,
        }

        impl Default for Params {
            fn default() -> Self {
                Self {
                    id: BuiltinInputParam { name: "id", ty: "UVec3" },
                    scores: StorageBufferParam { ty: "StorageBuffer<UInt>", read_only: false },
                    top_indices: StorageBufferParam { ty: "StorageBuffer<UInt>", read_only: false },
                    top_scores: StorageBufferParam { ty: "StorageBuffer<UInt>", read_only: false },
                    num_chunks: UniformParam { ty: "UInt" },
                    top_k: UniformParam { ty: "UInt" },
                    min_score: UniformParam { ty: "UInt" },
                    taken_mask: StorageBufferParam { ty: "StorageBuffer<UInt>", read_only: false },
                }
            }
        }

        pub const BINDINGS: &[BindingDesc] = &[
            BindingDesc { name: "scores", binding: 0, descriptor_set: 0, ty: "StorageBuffer<UInt>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "top_indices", binding: 1, descriptor_set: 0, ty: "StorageBuffer<UInt>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "top_scores", binding: 2, descriptor_set: 0, ty: "StorageBuffer<UInt>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "num_chunks", binding: 3, descriptor_set: 0, ty: "UInt", kind: BindingKind::Uniform, },
            BindingDesc { name: "top_k", binding: 4, descriptor_set: 0, ty: "UInt", kind: BindingKind::Uniform, },
            BindingDesc { name: "min_score", binding: 5, descriptor_set: 0, ty: "UInt", kind: BindingKind::Uniform, },
            BindingDesc { name: "taken_mask", binding: 6, descriptor_set: 0, ty: "StorageBuffer<UInt>", kind: BindingKind::StorageBuffer, },
        ];

        pub const SHADER: ShaderDesc = ShaderDesc { name: "SemanticWarpTopK", stage: ShaderStage::Compute, entry_point: "SemanticWarpTopK", output_type: "Void", bindings: BINDINGS, };

        pub fn descriptor() -> &'static ShaderDesc {
            &SHADER
        }

        pub fn descriptor_layout() -> Vec<BindingLayoutEntry> {
            BINDINGS
                .iter()
                .map(|binding| BindingLayoutEntry {
                    binding: binding.binding,
                    descriptor_set: binding.descriptor_set,
                    kind: binding.kind,
                    ty: binding.ty,
                })
                .collect()
        }

        pub fn dispatch<'a>(params: &'a Params, x: u32, y: u32, z: u32) -> DispatchCall<'a, Params> {
            DispatchCall {
                entry_point: "SemanticWarpTopK",
                stage: ShaderStage::Compute,
                size: DispatchSize { x, y, z },
                params,
            }
        }
    }

    pub mod semanticcoarseprefilter {
        use super::{
            BindingDesc, BindingKind, BindingLayoutEntry, BuiltinInputParam, DispatchCall,
            DispatchSize, LocalSizeParam, Sampler2DParam, ShaderDesc, ShaderStage,
            SpecializationConstantParam, StorageBufferParam, UniformParam,
        };

        #[derive(Debug, Clone)]
        pub struct Params {
            pub id: BuiltinInputParam,
            pub query_embed: StorageBufferParam,
            pub index_matrix: StorageBufferParam,
            pub candidate_mask: StorageBufferParam,
            pub dim: UniformParam,
            pub num_chunks: UniformParam,
            pub sig_stride: UniformParam,
            pub min_sig_match: UniformParam,
        }

        impl Default for Params {
            fn default() -> Self {
                Self {
                    id: BuiltinInputParam { name: "id", ty: "UVec3" },
                    query_embed: StorageBufferParam { ty: "StorageBuffer<u8>", read_only: false },
                    index_matrix: StorageBufferParam { ty: "StorageBuffer<u8>", read_only: false },
                    candidate_mask: StorageBufferParam { ty: "StorageBuffer<UInt>", read_only: false },
                    dim: UniformParam { ty: "UInt" },
                    num_chunks: UniformParam { ty: "UInt" },
                    sig_stride: UniformParam { ty: "UInt" },
                    min_sig_match: UniformParam { ty: "UInt" },
                }
            }
        }

        pub const BINDINGS: &[BindingDesc] = &[
            BindingDesc { name: "query_embed", binding: 0, descriptor_set: 0, ty: "StorageBuffer<u8>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "index_matrix", binding: 1, descriptor_set: 0, ty: "StorageBuffer<u8>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "candidate_mask", binding: 2, descriptor_set: 0, ty: "StorageBuffer<UInt>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "dim", binding: 3, descriptor_set: 0, ty: "UInt", kind: BindingKind::Uniform, },
            BindingDesc { name: "num_chunks", binding: 4, descriptor_set: 0, ty: "UInt", kind: BindingKind::Uniform, },
            BindingDesc { name: "sig_stride", binding: 5, descriptor_set: 0, ty: "UInt", kind: BindingKind::Uniform, },
            BindingDesc { name: "min_sig_match", binding: 6, descriptor_set: 0, ty: "UInt", kind: BindingKind::Uniform, },
        ];

        pub const SHADER: ShaderDesc = ShaderDesc { name: "SemanticCoarsePrefilter", stage: ShaderStage::Compute, entry_point: "SemanticCoarsePrefilter", output_type: "Void", bindings: BINDINGS, };

        pub fn descriptor() -> &'static ShaderDesc {
            &SHADER
        }

        pub fn descriptor_layout() -> Vec<BindingLayoutEntry> {
            BINDINGS
                .iter()
                .map(|binding| BindingLayoutEntry {
                    binding: binding.binding,
                    descriptor_set: binding.descriptor_set,
                    kind: binding.kind,
                    ty: binding.ty,
                })
                .collect()
        }

        pub fn dispatch<'a>(params: &'a Params, x: u32, y: u32, z: u32) -> DispatchCall<'a, Params> {
            DispatchCall {
                entry_point: "SemanticCoarsePrefilter",
                stage: ShaderStage::Compute,
                size: DispatchSize { x, y, z },
                params,
            }
        }
    }

    pub fn shaders() -> &'static [ShaderDesc] {
        &[
            semanticfusedscoretopk::SHADER,
            semanticbitpackdotproduct::SHADER,
            semanticwarptopk::SHADER,
            semanticcoarseprefilter::SHADER,
        ]
    }
}
