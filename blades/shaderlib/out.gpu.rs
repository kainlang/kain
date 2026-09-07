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

    pub mod blackholefragment {
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
            pub mouse: UniformParam,
            pub _pad: StorageBufferParam,
        }

        impl Default for Params {
            fn default() -> Self {
                Self {
                    uv: BuiltinInputParam { name: "uv", ty: "Vec2" },
                    time: UniformParam { ty: "Float" },
                    resolution: UniformParam { ty: "Vec2" },
                    mouse: UniformParam { ty: "Vec2" },
                    _pad: StorageBufferParam { ty: "StorageBuffer<Float>", read_only: false },
                }
            }
        }

        pub const BINDINGS: &[BindingDesc] = &[
            BindingDesc { name: "time", binding: 0, descriptor_set: 0, ty: "Float", kind: BindingKind::Uniform, },
            BindingDesc { name: "resolution", binding: 1, descriptor_set: 0, ty: "Vec2", kind: BindingKind::Uniform, },
            BindingDesc { name: "mouse", binding: 2, descriptor_set: 0, ty: "Vec2", kind: BindingKind::Uniform, },
            BindingDesc { name: "_pad", binding: 3, descriptor_set: 0, ty: "StorageBuffer<Float>", kind: BindingKind::StorageBuffer, },
        ];

        pub const SHADER: ShaderDesc = ShaderDesc { name: "BlackHoleFragment", stage: ShaderStage::Fragment, entry_point: "BlackHoleFragment", output_type: "Vec4", bindings: BINDINGS, };

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
                entry_point: "BlackHoleFragment",
                stage: ShaderStage::Fragment,
                size: DispatchSize { x, y, z },
                params,
            }
        }
    }

    pub fn shaders() -> &'static [ShaderDesc] {
        &[
            blackholefragment::SHADER,
        ]
    }
}
