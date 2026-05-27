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

    pub mod semanticcosinesimilarity {
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
            pub index_norms: StorageBufferParam,
            pub scores: StorageBufferParam,
            pub dim_data: StorageBufferParam,
            pub num_chunks_data: StorageBufferParam,
            pub query_norm_data: StorageBufferParam,
        }

        impl Default for Params {
            fn default() -> Self {
                Self {
                    id: BuiltinInputParam { name: "id", ty: "UVec3" },
                    query_embed: StorageBufferParam { ty: "StorageBuffer<Float>", read_only: false },
                    index_matrix: StorageBufferParam { ty: "StorageBuffer<Float>", read_only: false },
                    index_norms: StorageBufferParam { ty: "StorageBuffer<Float>", read_only: false },
                    scores: StorageBufferParam { ty: "StorageBuffer<Float>", read_only: false },
                    dim_data: StorageBufferParam { ty: "StorageBuffer<UInt>", read_only: false },
                    num_chunks_data: StorageBufferParam { ty: "StorageBuffer<UInt>", read_only: false },
                    query_norm_data: StorageBufferParam { ty: "StorageBuffer<Float>", read_only: false },
                }
            }
        }

        pub const BINDINGS: &[BindingDesc] = &[
            BindingDesc { name: "query_embed", binding: 0, descriptor_set: 0, ty: "StorageBuffer<Float>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "index_matrix", binding: 1, descriptor_set: 0, ty: "StorageBuffer<Float>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "index_norms", binding: 2, descriptor_set: 0, ty: "StorageBuffer<Float>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "scores", binding: 3, descriptor_set: 0, ty: "StorageBuffer<Float>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "dim_data", binding: 4, descriptor_set: 0, ty: "StorageBuffer<UInt>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "num_chunks_data", binding: 5, descriptor_set: 0, ty: "StorageBuffer<UInt>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "query_norm_data", binding: 6, descriptor_set: 0, ty: "StorageBuffer<Float>", kind: BindingKind::StorageBuffer, },
        ];

        pub const SHADER: ShaderDesc = ShaderDesc { name: "SemanticCosineSimilarity", stage: ShaderStage::Compute, entry_point: "SemanticCosineSimilarity", output_type: "Void", bindings: BINDINGS, };

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
                entry_point: "SemanticCosineSimilarity",
                stage: ShaderStage::Compute,
                size: DispatchSize { x, y, z },
                params,
            }
        }
    }

    pub fn shaders() -> &'static [ShaderDesc] {
        &[
            semanticcosinesimilarity::SHADER,
        ]
    }
}
