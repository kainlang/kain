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

    pub mod inceptionkernel {
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
            pub _pad: StorageBufferParam,
        }

        impl Default for Params {
            fn default() -> Self {
                Self {
                    id: BuiltinInputParam { name: "id", ty: "UVec3" },
                    src: StorageBufferParam { ty: "StorageBuffer<Float>", read_only: false },
                    dst: StorageBufferParam { ty: "StorageBuffer<Float>", read_only: false },
                    _pad: StorageBufferParam { ty: "StorageBuffer<Float>", read_only: false },
                }
            }
        }

        pub const BINDINGS: &[BindingDesc] = &[
            BindingDesc { name: "src", binding: 0, descriptor_set: 0, ty: "StorageBuffer<Float>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "dst", binding: 1, descriptor_set: 0, ty: "StorageBuffer<Float>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "_pad", binding: 2, descriptor_set: 0, ty: "StorageBuffer<Float>", kind: BindingKind::StorageBuffer, },
        ];

        pub const SHADER: ShaderDesc = ShaderDesc { name: "InceptionKernel", stage: ShaderStage::Compute, entry_point: "InceptionKernel", output_type: "Void", bindings: BINDINGS, };

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
                entry_point: "InceptionKernel",
                stage: ShaderStage::Compute,
                size: DispatchSize { x, y, z },
                params,
            }
        }
    }

    pub mod mandelbulbmesh {
        use super::{
            BindingDesc, BindingKind, BindingLayoutEntry, BuiltinInputParam, DispatchCall,
            DispatchSize, LocalSizeParam, Sampler2DParam, ShaderDesc, ShaderStage,
            SpecializationConstantParam, StorageBufferParam, UniformParam,
        };

        #[derive(Debug, Clone)]
        pub struct Params {
            pub out_positions: BuiltinInputParam,
            pub out_normals: BuiltinInputParam,
            pub out_indices: BuiltinInputParam,
            pub time: UniformParam,
            pub detail_level: UniformParam,
            pub _pad: StorageBufferParam,
        }

        impl Default for Params {
            fn default() -> Self {
                Self {
                    out_positions: BuiltinInputParam { name: "out_positions", ty: "Vec3" },
                    out_normals: BuiltinInputParam { name: "out_normals", ty: "Vec3" },
                    out_indices: BuiltinInputParam { name: "out_indices", ty: "UInt" },
                    time: UniformParam { ty: "Float" },
                    detail_level: UniformParam { ty: "Float" },
                    _pad: StorageBufferParam { ty: "StorageBuffer<Float>", read_only: false },
                }
            }
        }

        pub const BINDINGS: &[BindingDesc] = &[
            BindingDesc { name: "time", binding: 0, descriptor_set: 0, ty: "Float", kind: BindingKind::Uniform, },
            BindingDesc { name: "detail_level", binding: 1, descriptor_set: 0, ty: "Float", kind: BindingKind::Uniform, },
            BindingDesc { name: "_pad", binding: 2, descriptor_set: 0, ty: "StorageBuffer<Float>", kind: BindingKind::StorageBuffer, },
        ];

        pub const SHADER: ShaderDesc = ShaderDesc { name: "MandelbulbMesh", stage: ShaderStage::Mesh, entry_point: "MandelbulbMesh", output_type: "Void", bindings: BINDINGS, };

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
                entry_point: "MandelbulbMesh",
                stage: ShaderStage::Mesh,
                size: DispatchSize { x, y, z },
                params,
            }
        }
    }

    pub mod mandelbulbcull {
        use super::{
            BindingDesc, BindingKind, BindingLayoutEntry, BuiltinInputParam, DispatchCall,
            DispatchSize, LocalSizeParam, Sampler2DParam, ShaderDesc, ShaderStage,
            SpecializationConstantParam, StorageBufferParam, UniformParam,
        };

        #[derive(Debug, Clone)]
        pub struct Params {
            pub view_proj: StorageBufferParam,
            pub draw_count: UniformParam,
        }

        impl Default for Params {
            fn default() -> Self {
                Self {
                    view_proj: StorageBufferParam { ty: "StorageBuffer<Float>", read_only: false },
                    draw_count: UniformParam { ty: "UInt" },
                }
            }
        }

        pub const BINDINGS: &[BindingDesc] = &[
            BindingDesc { name: "view_proj", binding: 0, descriptor_set: 0, ty: "StorageBuffer<Float>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "draw_count", binding: 1, descriptor_set: 0, ty: "UInt", kind: BindingKind::Uniform, },
        ];

        pub const SHADER: ShaderDesc = ShaderDesc { name: "MandelbulbCull", stage: ShaderStage::Task, entry_point: "MandelbulbCull", output_type: "Void", bindings: BINDINGS, };

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
                entry_point: "MandelbulbCull",
                stage: ShaderStage::Task,
                size: DispatchSize { x, y, z },
                params,
            }
        }
    }

    pub mod universalraygen {
        use super::{
            BindingDesc, BindingKind, BindingLayoutEntry, BuiltinInputParam, DispatchCall,
            DispatchSize, LocalSizeParam, Sampler2DParam, ShaderDesc, ShaderStage,
            SpecializationConstantParam, StorageBufferParam, UniformParam,
        };

        #[derive(Debug, Clone)]
        pub struct Params {
            pub frame: UniformParam,
            pub seed: UniformParam,
            pub _pad: StorageBufferParam,
        }

        impl Default for Params {
            fn default() -> Self {
                Self {
                    frame: UniformParam { ty: "UInt" },
                    seed: UniformParam { ty: "Float" },
                    _pad: StorageBufferParam { ty: "StorageBuffer<Float>", read_only: false },
                }
            }
        }

        pub const BINDINGS: &[BindingDesc] = &[
            BindingDesc { name: "frame", binding: 0, descriptor_set: 0, ty: "UInt", kind: BindingKind::Uniform, },
            BindingDesc { name: "seed", binding: 1, descriptor_set: 0, ty: "Float", kind: BindingKind::Uniform, },
            BindingDesc { name: "_pad", binding: 2, descriptor_set: 0, ty: "StorageBuffer<Float>", kind: BindingKind::StorageBuffer, },
        ];

        pub const SHADER: ShaderDesc = ShaderDesc { name: "UniversalRayGen", stage: ShaderStage::RayGen, entry_point: "UniversalRayGen", output_type: "Vec4", bindings: BINDINGS, };

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
                entry_point: "UniversalRayGen",
                stage: ShaderStage::RayGen,
                size: DispatchSize { x, y, z },
                params,
            }
        }
    }

    pub mod mandelbulbhit {
        use super::{
            BindingDesc, BindingKind, BindingLayoutEntry, BuiltinInputParam, DispatchCall,
            DispatchSize, LocalSizeParam, Sampler2DParam, ShaderDesc, ShaderStage,
            SpecializationConstantParam, StorageBufferParam, UniformParam,
        };

        #[derive(Debug, Clone)]
        pub struct Params {
            pub hit_distance: UniformParam,
            pub hit_normal_x: UniformParam,
            pub hit_normal_y: UniformParam,
            pub hit_normal_z: UniformParam,
            pub _pad: StorageBufferParam,
        }

        impl Default for Params {
            fn default() -> Self {
                Self {
                    hit_distance: UniformParam { ty: "Float" },
                    hit_normal_x: UniformParam { ty: "Float" },
                    hit_normal_y: UniformParam { ty: "Float" },
                    hit_normal_z: UniformParam { ty: "Float" },
                    _pad: StorageBufferParam { ty: "StorageBuffer<Float>", read_only: false },
                }
            }
        }

        pub const BINDINGS: &[BindingDesc] = &[
            BindingDesc { name: "hit_distance", binding: 0, descriptor_set: 0, ty: "Float", kind: BindingKind::Uniform, },
            BindingDesc { name: "hit_normal_x", binding: 1, descriptor_set: 0, ty: "Float", kind: BindingKind::Uniform, },
            BindingDesc { name: "hit_normal_y", binding: 2, descriptor_set: 0, ty: "Float", kind: BindingKind::Uniform, },
            BindingDesc { name: "hit_normal_z", binding: 3, descriptor_set: 0, ty: "Float", kind: BindingKind::Uniform, },
            BindingDesc { name: "_pad", binding: 4, descriptor_set: 0, ty: "StorageBuffer<Float>", kind: BindingKind::StorageBuffer, },
        ];

        pub const SHADER: ShaderDesc = ShaderDesc { name: "MandelbulbHit", stage: ShaderStage::ClosestHit, entry_point: "MandelbulbHit", output_type: "Vec4", bindings: BINDINGS, };

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
                entry_point: "MandelbulbHit",
                stage: ShaderStage::ClosestHit,
                size: DispatchSize { x, y, z },
                params,
            }
        }
    }

    pub mod cosmicmicrowavebackground {
        use super::{
            BindingDesc, BindingKind, BindingLayoutEntry, BuiltinInputParam, DispatchCall,
            DispatchSize, LocalSizeParam, Sampler2DParam, ShaderDesc, ShaderStage,
            SpecializationConstantParam, StorageBufferParam, UniformParam,
        };

        #[derive(Debug, Clone)]
        pub struct Params {
            pub ray_dir_x: UniformParam,
            pub ray_dir_y: UniformParam,
            pub ray_dir_z: UniformParam,
            pub _pad: StorageBufferParam,
        }

        impl Default for Params {
            fn default() -> Self {
                Self {
                    ray_dir_x: UniformParam { ty: "Float" },
                    ray_dir_y: UniformParam { ty: "Float" },
                    ray_dir_z: UniformParam { ty: "Float" },
                    _pad: StorageBufferParam { ty: "StorageBuffer<Float>", read_only: false },
                }
            }
        }

        pub const BINDINGS: &[BindingDesc] = &[
            BindingDesc { name: "ray_dir_x", binding: 0, descriptor_set: 0, ty: "Float", kind: BindingKind::Uniform, },
            BindingDesc { name: "ray_dir_y", binding: 1, descriptor_set: 0, ty: "Float", kind: BindingKind::Uniform, },
            BindingDesc { name: "ray_dir_z", binding: 2, descriptor_set: 0, ty: "Float", kind: BindingKind::Uniform, },
            BindingDesc { name: "_pad", binding: 3, descriptor_set: 0, ty: "StorageBuffer<Float>", kind: BindingKind::StorageBuffer, },
        ];

        pub const SHADER: ShaderDesc = ShaderDesc { name: "CosmicMicrowaveBackground", stage: ShaderStage::Miss, entry_point: "CosmicMicrowaveBackground", output_type: "Vec4", bindings: BINDINGS, };

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
                entry_point: "CosmicMicrowaveBackground",
                stage: ShaderStage::Miss,
                size: DispatchSize { x, y, z },
                params,
            }
        }
    }

    pub mod fractaldensitytest {
        use super::{
            BindingDesc, BindingKind, BindingLayoutEntry, BuiltinInputParam, DispatchCall,
            DispatchSize, LocalSizeParam, Sampler2DParam, ShaderDesc, ShaderStage,
            SpecializationConstantParam, StorageBufferParam, UniformParam,
        };

        #[derive(Debug, Clone)]
        pub struct Params {
            pub density: UniformParam,
            pub threshold: UniformParam,
            pub _pad: StorageBufferParam,
        }

        impl Default for Params {
            fn default() -> Self {
                Self {
                    density: UniformParam { ty: "Float" },
                    threshold: UniformParam { ty: "Float" },
                    _pad: StorageBufferParam { ty: "StorageBuffer<Float>", read_only: false },
                }
            }
        }

        pub const BINDINGS: &[BindingDesc] = &[
            BindingDesc { name: "density", binding: 0, descriptor_set: 0, ty: "Float", kind: BindingKind::Uniform, },
            BindingDesc { name: "threshold", binding: 1, descriptor_set: 0, ty: "Float", kind: BindingKind::Uniform, },
            BindingDesc { name: "_pad", binding: 2, descriptor_set: 0, ty: "StorageBuffer<Float>", kind: BindingKind::StorageBuffer, },
        ];

        pub const SHADER: ShaderDesc = ShaderDesc { name: "FractalDensityTest", stage: ShaderStage::AnyHit, entry_point: "FractalDensityTest", output_type: "Vec4", bindings: BINDINGS, };

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
                entry_point: "FractalDensityTest",
                stage: ShaderStage::AnyHit,
                size: DispatchSize { x, y, z },
                params,
            }
        }
    }

    pub mod tesseractintersection {
        use super::{
            BindingDesc, BindingKind, BindingLayoutEntry, BuiltinInputParam, DispatchCall,
            DispatchSize, LocalSizeParam, Sampler2DParam, ShaderDesc, ShaderStage,
            SpecializationConstantParam, StorageBufferParam, UniformParam,
        };

        #[derive(Debug, Clone)]
        pub struct Params {
            pub tesseract_scale: UniformParam,
            pub rotation_angle: UniformParam,
            pub _pad: StorageBufferParam,
        }

        impl Default for Params {
            fn default() -> Self {
                Self {
                    tesseract_scale: UniformParam { ty: "Float" },
                    rotation_angle: UniformParam { ty: "Float" },
                    _pad: StorageBufferParam { ty: "StorageBuffer<Float>", read_only: false },
                }
            }
        }

        pub const BINDINGS: &[BindingDesc] = &[
            BindingDesc { name: "tesseract_scale", binding: 0, descriptor_set: 0, ty: "Float", kind: BindingKind::Uniform, },
            BindingDesc { name: "rotation_angle", binding: 1, descriptor_set: 0, ty: "Float", kind: BindingKind::Uniform, },
            BindingDesc { name: "_pad", binding: 2, descriptor_set: 0, ty: "StorageBuffer<Float>", kind: BindingKind::StorageBuffer, },
        ];

        pub const SHADER: ShaderDesc = ShaderDesc { name: "TesseractIntersection", stage: ShaderStage::Intersection, entry_point: "TesseractIntersection", output_type: "Vec4", bindings: BINDINGS, };

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
                entry_point: "TesseractIntersection",
                stage: ShaderStage::Intersection,
                size: DispatchSize { x, y, z },
                params,
            }
        }
    }

    pub mod fractalutility {
        use super::{
            BindingDesc, BindingKind, BindingLayoutEntry, BuiltinInputParam, DispatchCall,
            DispatchSize, LocalSizeParam, Sampler2DParam, ShaderDesc, ShaderStage,
            SpecializationConstantParam, StorageBufferParam, UniformParam,
        };

        #[derive(Debug, Clone)]
        pub struct Params {
            pub input_a: UniformParam,
            pub input_b: UniformParam,
            pub mode: UniformParam,
            pub _pad: StorageBufferParam,
        }

        impl Default for Params {
            fn default() -> Self {
                Self {
                    input_a: UniformParam { ty: "Float" },
                    input_b: UniformParam { ty: "Float" },
                    mode: UniformParam { ty: "Float" },
                    _pad: StorageBufferParam { ty: "StorageBuffer<Float>", read_only: false },
                }
            }
        }

        pub const BINDINGS: &[BindingDesc] = &[
            BindingDesc { name: "input_a", binding: 0, descriptor_set: 0, ty: "Float", kind: BindingKind::Uniform, },
            BindingDesc { name: "input_b", binding: 1, descriptor_set: 0, ty: "Float", kind: BindingKind::Uniform, },
            BindingDesc { name: "mode", binding: 2, descriptor_set: 0, ty: "Float", kind: BindingKind::Uniform, },
            BindingDesc { name: "_pad", binding: 3, descriptor_set: 0, ty: "StorageBuffer<Float>", kind: BindingKind::StorageBuffer, },
        ];

        pub const SHADER: ShaderDesc = ShaderDesc { name: "FractalUtility", stage: ShaderStage::Callable, entry_point: "FractalUtility", output_type: "Vec4", bindings: BINDINGS, };

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
                entry_point: "FractalUtility",
                stage: ShaderStage::Callable,
                size: DispatchSize { x, y, z },
                params,
            }
        }
    }

    pub mod rasterfallback {
        use super::{
            BindingDesc, BindingKind, BindingLayoutEntry, BuiltinInputParam, DispatchCall,
            DispatchSize, LocalSizeParam, Sampler2DParam, ShaderDesc, ShaderStage,
            SpecializationConstantParam, StorageBufferParam, UniformParam,
        };

        #[derive(Debug, Clone)]
        pub struct Params {
            pub position: BuiltinInputParam,
            pub uv: BuiltinInputParam,
            pub model: UniformParam,
            pub view: UniformParam,
            pub proj: UniformParam,
            pub _pad: StorageBufferParam,
        }

        impl Default for Params {
            fn default() -> Self {
                Self {
                    position: BuiltinInputParam { name: "position", ty: "Vec3" },
                    uv: BuiltinInputParam { name: "uv", ty: "Vec2" },
                    model: UniformParam { ty: "Vec4" },
                    view: UniformParam { ty: "Vec4" },
                    proj: UniformParam { ty: "Vec4" },
                    _pad: StorageBufferParam { ty: "StorageBuffer<Float>", read_only: false },
                }
            }
        }

        pub const BINDINGS: &[BindingDesc] = &[
            BindingDesc { name: "model", binding: 0, descriptor_set: 0, ty: "Vec4", kind: BindingKind::Uniform, },
            BindingDesc { name: "view", binding: 1, descriptor_set: 0, ty: "Vec4", kind: BindingKind::Uniform, },
            BindingDesc { name: "proj", binding: 2, descriptor_set: 0, ty: "Vec4", kind: BindingKind::Uniform, },
            BindingDesc { name: "_pad", binding: 3, descriptor_set: 0, ty: "StorageBuffer<Float>", kind: BindingKind::StorageBuffer, },
        ];

        pub const SHADER: ShaderDesc = ShaderDesc { name: "RasterFallback", stage: ShaderStage::Vertex, entry_point: "RasterFallback", output_type: "Vec4", bindings: BINDINGS, };

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
                entry_point: "RasterFallback",
                stage: ShaderStage::Vertex,
                size: DispatchSize { x, y, z },
                params,
            }
        }
    }

    pub mod proceduralreality {
        use super::{
            BindingDesc, BindingKind, BindingLayoutEntry, BuiltinInputParam, DispatchCall,
            DispatchSize, LocalSizeParam, Sampler2DParam, ShaderDesc, ShaderStage,
            SpecializationConstantParam, StorageBufferParam, UniformParam,
        };

        #[derive(Debug, Clone)]
        pub struct Params {
            pub uv: BuiltinInputParam,
            pub time: UniformParam,
            pub mouse: UniformParam,
            pub _pad: StorageBufferParam,
        }

        impl Default for Params {
            fn default() -> Self {
                Self {
                    uv: BuiltinInputParam { name: "uv", ty: "Vec2" },
                    time: UniformParam { ty: "Float" },
                    mouse: UniformParam { ty: "Vec2" },
                    _pad: StorageBufferParam { ty: "StorageBuffer<Float>", read_only: false },
                }
            }
        }

        pub const BINDINGS: &[BindingDesc] = &[
            BindingDesc { name: "time", binding: 0, descriptor_set: 0, ty: "Float", kind: BindingKind::Uniform, },
            BindingDesc { name: "mouse", binding: 1, descriptor_set: 0, ty: "Vec2", kind: BindingKind::Uniform, },
            BindingDesc { name: "_pad", binding: 2, descriptor_set: 0, ty: "StorageBuffer<Float>", kind: BindingKind::StorageBuffer, },
        ];

        pub const SHADER: ShaderDesc = ShaderDesc { name: "ProceduralReality", stage: ShaderStage::Fragment, entry_point: "ProceduralReality", output_type: "Vec4", bindings: BINDINGS, };

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
                entry_point: "ProceduralReality",
                stage: ShaderStage::Fragment,
                size: DispatchSize { x, y, z },
                params,
            }
        }
    }

    pub mod indirectcontroller {
        use super::{
            BindingDesc, BindingKind, BindingLayoutEntry, BuiltinInputParam, DispatchCall,
            DispatchSize, LocalSizeParam, Sampler2DParam, ShaderDesc, ShaderStage,
            SpecializationConstantParam, StorageBufferParam, UniformParam,
        };

        #[derive(Debug, Clone)]
        pub struct Params {
            pub id: BuiltinInputParam,
            pub indirect_buf: StorageBufferParam,
            pub workload_size: UniformParam,
        }

        impl Default for Params {
            fn default() -> Self {
                Self {
                    id: BuiltinInputParam { name: "id", ty: "UVec3" },
                    indirect_buf: StorageBufferParam { ty: "StorageBuffer<UInt>", read_only: false },
                    workload_size: UniformParam { ty: "UInt" },
                }
            }
        }

        pub const BINDINGS: &[BindingDesc] = &[
            BindingDesc { name: "indirect_buf", binding: 0, descriptor_set: 0, ty: "StorageBuffer<UInt>", kind: BindingKind::StorageBuffer, },
            BindingDesc { name: "workload_size", binding: 1, descriptor_set: 0, ty: "UInt", kind: BindingKind::Uniform, },
        ];

        pub const SHADER: ShaderDesc = ShaderDesc { name: "IndirectController", stage: ShaderStage::Compute, entry_point: "IndirectController", output_type: "Void", bindings: BINDINGS, };

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
                entry_point: "IndirectController",
                stage: ShaderStage::Compute,
                size: DispatchSize { x, y, z },
                params,
            }
        }
    }

    pub fn shaders() -> &'static [ShaderDesc] {
        &[
            inceptionkernel::SHADER,
            mandelbulbmesh::SHADER,
            mandelbulbcull::SHADER,
            universalraygen::SHADER,
            mandelbulbhit::SHADER,
            cosmicmicrowavebackground::SHADER,
            fractaldensitytest::SHADER,
            tesseractintersection::SHADER,
            fractalutility::SHADER,
            rasterfallback::SHADER,
            proceduralreality::SHADER,
            indirectcontroller::SHADER,
        ]
    }
}
