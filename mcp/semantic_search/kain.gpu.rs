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

    pub mod semanticpackedscore {
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
            BindingDesc { name: "scores", binding: 3, descriptor_set: 0, ty: "StorageBuffer<UInt>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "dim", binding: 4, descriptor_set: 0, ty: "UInt", kind: BindingKind::Uniform, },
            BindingDesc { name: "num_chunks", binding: 5, descriptor_set: 0, ty: "UInt", kind: BindingKind::Uniform, },
        ];

        pub const SHADER: ShaderDesc = ShaderDesc { name: "SemanticPackedScore", stage: ShaderStage::Compute, entry_point: "SemanticPackedScore", output_type: "Void", bindings: BINDINGS, };

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
                entry_point: "SemanticPackedScore",
                stage: ShaderStage::Compute,
                size: DispatchSize { x, y, z },
                params,
            }
        }
    }

    pub mod semanticgputopk {
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
        ];

        pub const SHADER: ShaderDesc = ShaderDesc { name: "SemanticGpuTopK", stage: ShaderStage::Compute, entry_point: "SemanticGpuTopK", output_type: "Void", bindings: BINDINGS, };

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
                entry_point: "SemanticGpuTopK",
                stage: ShaderStage::Compute,
                size: DispatchSize { x, y, z },
                params,
            }
        }
    }

    pub fn shaders() -> &'static [ShaderDesc] {
        &[
            semanticpackedscore::SHADER,
            semanticgputopk::SHADER,
        ]
    }
}
