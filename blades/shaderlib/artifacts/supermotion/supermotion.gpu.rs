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

    pub mod supermotioncompute {
        use super::{
            BindingDesc, BindingKind, BindingLayoutEntry, BuiltinInputParam, DispatchCall,
            DispatchSize, LocalSizeParam, Sampler2DParam, ShaderDesc, ShaderStage,
            SpecializationConstantParam, StorageBufferParam, UniformParam,
        };

        #[derive(Debug, Clone)]
        pub struct Params {
            pub id: BuiltinInputParam,
            pub in_pos: StorageBufferParam,
            pub in_rot: StorageBufferParam,
            pub in_scl: StorageBufferParam,
            pub out_pos: StorageBufferParam,
            pub out_rot: StorageBufferParam,
            pub out_scl: StorageBufferParam,
            pub mod_type: StorageBufferParam,
            pub params_a: StorageBufferParam,
            pub params_b: StorageBufferParam,
            pub params_c: StorageBufferParam,
            pub instance_count: UniformParam,
            pub time_sec: UniformParam,
            pub global_intensity: UniformParam,
            pub random_seed: UniformParam,
            pub eps: UniformParam,
            pub _pad: StorageBufferParam,
        }

        impl Default for Params {
            fn default() -> Self {
                Self {
                    id: BuiltinInputParam { name: "id", ty: "UVec3" },
                    in_pos: StorageBufferParam { ty: "StorageBuffer<Vec4>", read_only: false },
                    in_rot: StorageBufferParam { ty: "StorageBuffer<Vec4>", read_only: false },
                    in_scl: StorageBufferParam { ty: "StorageBuffer<Vec4>", read_only: false },
                    out_pos: StorageBufferParam { ty: "StorageBuffer<Vec4>", read_only: false },
                    out_rot: StorageBufferParam { ty: "StorageBuffer<Vec4>", read_only: false },
                    out_scl: StorageBufferParam { ty: "StorageBuffer<Vec4>", read_only: false },
                    mod_type: StorageBufferParam { ty: "StorageBuffer<Int>", read_only: false },
                    params_a: StorageBufferParam { ty: "StorageBuffer<Vec4>", read_only: false },
                    params_b: StorageBufferParam { ty: "StorageBuffer<Vec4>", read_only: false },
                    params_c: StorageBufferParam { ty: "StorageBuffer<Vec4>", read_only: false },
                    instance_count: UniformParam { ty: "UInt" },
                    time_sec: UniformParam { ty: "Float" },
                    global_intensity: UniformParam { ty: "Float" },
                    random_seed: UniformParam { ty: "Float" },
                    eps: UniformParam { ty: "Float" },
                    _pad: StorageBufferParam { ty: "StorageBuffer<Float>", read_only: false },
                }
            }
        }

        pub const BINDINGS: &[BindingDesc] = &[
            BindingDesc { name: "in_pos", binding: 0, descriptor_set: 0, ty: "StorageBuffer<Vec4>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "in_rot", binding: 1, descriptor_set: 0, ty: "StorageBuffer<Vec4>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "in_scl", binding: 2, descriptor_set: 0, ty: "StorageBuffer<Vec4>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "out_pos", binding: 3, descriptor_set: 0, ty: "StorageBuffer<Vec4>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "out_rot", binding: 4, descriptor_set: 0, ty: "StorageBuffer<Vec4>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "out_scl", binding: 5, descriptor_set: 0, ty: "StorageBuffer<Vec4>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "mod_type", binding: 6, descriptor_set: 0, ty: "StorageBuffer<Int>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "params_a", binding: 7, descriptor_set: 0, ty: "StorageBuffer<Vec4>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "params_b", binding: 8, descriptor_set: 0, ty: "StorageBuffer<Vec4>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "params_c", binding: 9, descriptor_set: 0, ty: "StorageBuffer<Vec4>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "instance_count", binding: 10, descriptor_set: 0, ty: "UInt", kind: BindingKind::Uniform, },
            BindingDesc { name: "time_sec", binding: 11, descriptor_set: 0, ty: "Float", kind: BindingKind::Uniform, },
            BindingDesc { name: "global_intensity", binding: 12, descriptor_set: 0, ty: "Float", kind: BindingKind::Uniform, },
            BindingDesc { name: "random_seed", binding: 13, descriptor_set: 0, ty: "Float", kind: BindingKind::Uniform, },
            BindingDesc { name: "eps", binding: 14, descriptor_set: 0, ty: "Float", kind: BindingKind::Uniform, },
            BindingDesc { name: "_pad", binding: 15, descriptor_set: 0, ty: "StorageBuffer<Float>", kind: BindingKind::StorageBuffer, },
        ];

        pub const SHADER: ShaderDesc = ShaderDesc { name: "SupermotionCompute", stage: ShaderStage::Compute, entry_point: "SupermotionCompute", output_type: "Void", bindings: BINDINGS, };

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
                entry_point: "SupermotionCompute",
                stage: ShaderStage::Compute,
                size: DispatchSize { x, y, z },
                params,
            }
        }
    }

    pub mod supermotionvertex {
        use super::{
            BindingDesc, BindingKind, BindingLayoutEntry, BuiltinInputParam, DispatchCall,
            DispatchSize, LocalSizeParam, Sampler2DParam, ShaderDesc, ShaderStage,
            SpecializationConstantParam, StorageBufferParam, UniformParam,
        };

        #[derive(Debug, Clone)]
        pub struct Params {
            pub position: BuiltinInputParam,
            pub uv: BuiltinInputParam,
            pub instance_pos: StorageBufferParam,
            pub instance_rot: StorageBufferParam,
            pub instance_scl: StorageBufferParam,
            pub instance_id: UniformParam,
            pub _pad: StorageBufferParam,
        }

        impl Default for Params {
            fn default() -> Self {
                Self {
                    position: BuiltinInputParam { name: "position", ty: "Vec3" },
                    uv: BuiltinInputParam { name: "uv", ty: "Vec2" },
                    instance_pos: StorageBufferParam { ty: "StorageBuffer<Vec4>", read_only: false },
                    instance_rot: StorageBufferParam { ty: "StorageBuffer<Vec4>", read_only: false },
                    instance_scl: StorageBufferParam { ty: "StorageBuffer<Vec4>", read_only: false },
                    instance_id: UniformParam { ty: "UInt" },
                    _pad: StorageBufferParam { ty: "StorageBuffer<Float>", read_only: false },
                }
            }
        }

        pub const BINDINGS: &[BindingDesc] = &[
            BindingDesc { name: "instance_pos", binding: 0, descriptor_set: 0, ty: "StorageBuffer<Vec4>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "instance_rot", binding: 1, descriptor_set: 0, ty: "StorageBuffer<Vec4>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "instance_scl", binding: 2, descriptor_set: 0, ty: "StorageBuffer<Vec4>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "instance_id", binding: 3, descriptor_set: 0, ty: "UInt", kind: BindingKind::Uniform, },
            BindingDesc { name: "_pad", binding: 4, descriptor_set: 0, ty: "StorageBuffer<Float>", kind: BindingKind::StorageBuffer, },
        ];

        pub const SHADER: ShaderDesc = ShaderDesc { name: "SupermotionVertex", stage: ShaderStage::Vertex, entry_point: "SupermotionVertex", output_type: "Vec4", bindings: BINDINGS, };

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
                entry_point: "SupermotionVertex",
                stage: ShaderStage::Vertex,
                size: DispatchSize { x, y, z },
                params,
            }
        }
    }

    pub mod supermotionfragment {
        use super::{
            BindingDesc, BindingKind, BindingLayoutEntry, BuiltinInputParam, DispatchCall,
            DispatchSize, LocalSizeParam, Sampler2DParam, ShaderDesc, ShaderStage,
            SpecializationConstantParam, StorageBufferParam, UniformParam,
        };

        #[derive(Debug, Clone)]
        pub struct Params {
            pub uv: BuiltinInputParam,
            pub time: UniformParam,
            pub resolution: UniformParam,
            pub instance_count: UniformParam,
            pub instance_pos: StorageBufferParam,
            pub instance_rot: StorageBufferParam,
            pub instance_scl: StorageBufferParam,
            pub mod_type: StorageBufferParam,
            pub _pad: StorageBufferParam,
        }

        impl Default for Params {
            fn default() -> Self {
                Self {
                    uv: BuiltinInputParam { name: "uv", ty: "Vec2" },
                    time: UniformParam { ty: "Float" },
                    resolution: UniformParam { ty: "Vec2" },
                    instance_count: UniformParam { ty: "UInt" },
                    instance_pos: StorageBufferParam { ty: "StorageBuffer<Vec4>", read_only: false },
                    instance_rot: StorageBufferParam { ty: "StorageBuffer<Vec4>", read_only: false },
                    instance_scl: StorageBufferParam { ty: "StorageBuffer<Vec4>", read_only: false },
                    mod_type: StorageBufferParam { ty: "StorageBuffer<Int>", read_only: false },
                    _pad: StorageBufferParam { ty: "StorageBuffer<Float>", read_only: false },
                }
            }
        }

        pub const BINDINGS: &[BindingDesc] = &[
            BindingDesc { name: "time", binding: 0, descriptor_set: 0, ty: "Float", kind: BindingKind::Uniform, },
            BindingDesc { name: "resolution", binding: 1, descriptor_set: 0, ty: "Vec2", kind: BindingKind::Uniform, },
            BindingDesc { name: "instance_count", binding: 2, descriptor_set: 0, ty: "UInt", kind: BindingKind::Uniform, },
            BindingDesc { name: "instance_pos", binding: 3, descriptor_set: 0, ty: "StorageBuffer<Vec4>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "instance_rot", binding: 4, descriptor_set: 0, ty: "StorageBuffer<Vec4>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "instance_scl", binding: 5, descriptor_set: 0, ty: "StorageBuffer<Vec4>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "mod_type", binding: 6, descriptor_set: 0, ty: "StorageBuffer<Int>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "_pad", binding: 7, descriptor_set: 0, ty: "StorageBuffer<Float>", kind: BindingKind::StorageBuffer, },
        ];

        pub const SHADER: ShaderDesc = ShaderDesc { name: "SupermotionFragment", stage: ShaderStage::Fragment, entry_point: "SupermotionFragment", output_type: "Vec4", bindings: BINDINGS, };

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
                entry_point: "SupermotionFragment",
                stage: ShaderStage::Fragment,
                size: DispatchSize { x, y, z },
                params,
            }
        }
    }

    pub mod supermotionindirect {
        use super::{
            BindingDesc, BindingKind, BindingLayoutEntry, BuiltinInputParam, DispatchCall,
            DispatchSize, LocalSizeParam, Sampler2DParam, ShaderDesc, ShaderStage,
            SpecializationConstantParam, StorageBufferParam, UniformParam,
        };

        #[derive(Debug, Clone)]
        pub struct Params {
            pub id: BuiltinInputParam,
            pub indirect_buf: StorageBufferParam,
            pub instance_count: UniformParam,
            pub _pad: StorageBufferParam,
        }

        impl Default for Params {
            fn default() -> Self {
                Self {
                    id: BuiltinInputParam { name: "id", ty: "UVec3" },
                    indirect_buf: StorageBufferParam { ty: "StorageBuffer<UInt>", read_only: false },
                    instance_count: UniformParam { ty: "UInt" },
                    _pad: StorageBufferParam { ty: "StorageBuffer<Float>", read_only: false },
                }
            }
        }

        pub const BINDINGS: &[BindingDesc] = &[
            BindingDesc { name: "indirect_buf", binding: 0, descriptor_set: 0, ty: "StorageBuffer<UInt>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "instance_count", binding: 1, descriptor_set: 0, ty: "UInt", kind: BindingKind::Uniform, },
            BindingDesc { name: "_pad", binding: 2, descriptor_set: 0, ty: "StorageBuffer<Float>", kind: BindingKind::StorageBuffer, },
        ];

        pub const SHADER: ShaderDesc = ShaderDesc { name: "SupermotionIndirect", stage: ShaderStage::Compute, entry_point: "SupermotionIndirect", output_type: "Void", bindings: BINDINGS, };

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
                entry_point: "SupermotionIndirect",
                stage: ShaderStage::Compute,
                size: DispatchSize { x, y, z },
                params,
            }
        }
    }

    pub fn shaders() -> &'static [ShaderDesc] {
        &[
            supermotioncompute::SHADER,
            supermotionvertex::SHADER,
            supermotionfragment::SHADER,
            supermotionindirect::SHADER,
        ]
    }
}
