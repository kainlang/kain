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

    pub mod smokeparticlestep {
        use super::{
            BindingDesc, BindingKind, BindingLayoutEntry, BuiltinInputParam, DispatchCall,
            DispatchSize, LocalSizeParam, Sampler2DParam, ShaderDesc, ShaderStage,
            SpecializationConstantParam, StorageBufferParam, UniformParam,
        };

        #[derive(Debug, Clone)]
        pub struct Params {
            pub id: BuiltinInputParam,
            pub particles: StorageBufferParam,
            pub field: StorageBufferParam,
        }

        impl Default for Params {
            fn default() -> Self {
                Self {
                    id: BuiltinInputParam { name: "id", ty: "UVec3" },
                    particles: StorageBufferParam { ty: "StorageBuffer<Vec4>", read_only: false },
                    field: StorageBufferParam { ty: "StorageBuffer<Vec4>", read_only: false },
                }
            }
        }

        pub const BINDINGS: &[BindingDesc] = &[
            BindingDesc { name: "particles", binding: 0, descriptor_set: 0, ty: "StorageBuffer<Vec4>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "field", binding: 1, descriptor_set: 0, ty: "StorageBuffer<Vec4>", kind: BindingKind::StorageBuffer, },
        ];

        pub const SHADER: ShaderDesc = ShaderDesc { name: "SmokeParticleStep", stage: ShaderStage::Compute, entry_point: "SmokeParticleStep", output_type: "Vec4", bindings: BINDINGS, };

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
                entry_point: "SmokeParticleStep",
                stage: ShaderStage::Compute,
                size: DispatchSize { x, y, z },
                params,
            }
        }
    }

    pub mod smokereductionkernel {
        use super::{
            BindingDesc, BindingKind, BindingLayoutEntry, BuiltinInputParam, DispatchCall,
            DispatchSize, LocalSizeParam, Sampler2DParam, ShaderDesc, ShaderStage,
            SpecializationConstantParam, StorageBufferParam, UniformParam,
        };

        #[derive(Debug, Clone)]
        pub struct Params {
            pub id: BuiltinInputParam,
            pub src: StorageBufferParam,
            pub dst: StorageBufferParam,
        }

        impl Default for Params {
            fn default() -> Self {
                Self {
                    id: BuiltinInputParam { name: "id", ty: "UVec3" },
                    src: StorageBufferParam { ty: "StorageBuffer<Float>", read_only: false },
                    dst: StorageBufferParam { ty: "StorageBuffer<Float>", read_only: false },
                }
            }
        }

        pub const BINDINGS: &[BindingDesc] = &[
            BindingDesc { name: "src", binding: 0, descriptor_set: 0, ty: "StorageBuffer<Float>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "dst", binding: 1, descriptor_set: 0, ty: "StorageBuffer<Float>", kind: BindingKind::StorageBuffer, },
        ];

        pub const SHADER: ShaderDesc = ShaderDesc { name: "SmokeReductionKernel", stage: ShaderStage::Compute, entry_point: "SmokeReductionKernel", output_type: "Vec4", bindings: BINDINGS, };

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
                entry_point: "SmokeReductionKernel",
                stage: ShaderStage::Compute,
                size: DispatchSize { x, y, z },
                params,
            }
        }
    }

    pub mod smokeorchestratekernel {
        use super::{
            BindingDesc, BindingKind, BindingLayoutEntry, BuiltinInputParam, DispatchCall,
            DispatchSize, LocalSizeParam, Sampler2DParam, ShaderDesc, ShaderStage,
            SpecializationConstantParam, StorageBufferParam, UniformParam,
        };

        #[derive(Debug, Clone)]
        pub struct Params {
            pub id: BuiltinInputParam,
            pub src: StorageBufferParam,
            pub dst: StorageBufferParam,
        }

        impl Default for Params {
            fn default() -> Self {
                Self {
                    id: BuiltinInputParam { name: "id", ty: "UVec3" },
                    src: StorageBufferParam { ty: "StorageBuffer<UInt>", read_only: false },
                    dst: StorageBufferParam { ty: "StorageBuffer<UInt>", read_only: false },
                }
            }
        }

        pub const BINDINGS: &[BindingDesc] = &[
            BindingDesc { name: "src", binding: 0, descriptor_set: 0, ty: "StorageBuffer<UInt>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "dst", binding: 1, descriptor_set: 0, ty: "StorageBuffer<UInt>", kind: BindingKind::StorageBuffer, },
        ];

        pub const SHADER: ShaderDesc = ShaderDesc { name: "SmokeOrchestrateKernel", stage: ShaderStage::Compute, entry_point: "SmokeOrchestrateKernel", output_type: "Void", bindings: BINDINGS, };

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
                entry_point: "SmokeOrchestrateKernel",
                stage: ShaderStage::Compute,
                size: DispatchSize { x, y, z },
                params,
            }
        }
    }

    pub fn shaders() -> &'static [ShaderDesc] {
        &[
            smokeparticlestep::SHADER,
            smokereductionkernel::SHADER,
            smokeorchestratekernel::SHADER,
        ]
    }
}
