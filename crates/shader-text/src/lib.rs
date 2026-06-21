//! Shared text-shader lowering helpers for Kain GPU backends.
//!
//! SPIR-V stays the canonical native shader payload. This crate owns the
//! data-driven pieces shared by derived text backends such as HLSL, WGSL, and
//! USF: type mapping, resource classification, identifier policy, and the
//! direct HLSL/WGSL emitters used by `crates/gpu`.

use kain_core::ast::{
    BinaryOp, Block, CallArg, ElseBranch, Expr, Pattern, ShaderStage, Stmt, Type, UnaryOp,
};
use kain_core::error::{KainError, KainResult};
use kain_core::span::Span;
use kain_core::types::{TypedItem, TypedProgram, TypedShader};
use once_cell::sync::Lazy;
use std::collections::{HashMap, HashSet};

const DEFAULT_WORKGROUP_SIZE: [u32; 3] = [8, 8, 1];

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum TextShaderBackend {
    Hlsl,
    Wgsl,
    Usf,
}

impl TextShaderBackend {
    fn label(self) -> &'static str {
        match self {
            Self::Hlsl => "HLSL",
            Self::Wgsl => "WGSL",
            Self::Usf => "USF",
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum ScalarKind {
    F32,
    I32,
    U32,
    Bool,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum ShaderValueType {
    Void,
    Scalar(ScalarKind),
    Vector(ScalarKind, usize),
    Matrix(usize, usize),
    StorageBuffer(Box<ShaderValueType>),
    Texture2D,
    Texture3D,
    TextureCube,
    Sampler,
    Unknown(String),
}

impl ShaderValueType {
    pub fn from_ast_type(ty: &Type) -> Self {
        match ty {
            Type::Named { name, generics, .. } => match name.as_str() {
                "Void" => Self::Void,
                "Float" | "f32" => Self::Scalar(ScalarKind::F32),
                "Int" | "i32" => Self::Scalar(ScalarKind::I32),
                "UInt" | "u32" => Self::Scalar(ScalarKind::U32),
                "Bool" => Self::Scalar(ScalarKind::Bool),
                "Vec2" => Self::Vector(ScalarKind::F32, 2),
                "Vec3" | "Vec3A" => Self::Vector(ScalarKind::F32, 3),
                "Vec4" => Self::Vector(ScalarKind::F32, 4),
                "IVec2" => Self::Vector(ScalarKind::I32, 2),
                "IVec3" => Self::Vector(ScalarKind::I32, 3),
                "IVec4" => Self::Vector(ScalarKind::I32, 4),
                "UVec2" => Self::Vector(ScalarKind::U32, 2),
                "UVec3" => Self::Vector(ScalarKind::U32, 3),
                "UVec4" => Self::Vector(ScalarKind::U32, 4),
                "Mat2" => Self::Matrix(2, 2),
                "Mat3" => Self::Matrix(3, 3),
                "Mat4" => Self::Matrix(4, 4),
                "Sampler2D" | "Texture2D" => Self::Texture2D,
                "Sampler3D" | "Texture3D" => Self::Texture3D,
                "SamplerCube" | "TextureCube" => Self::TextureCube,
                "Sampler" | "SamplerState" => Self::Sampler,
                "StorageBuffer" | "StructuredBuffer" | "Buffer" | "RWStructuredBuffer"
                | "RWBuffer" => {
                    let inner = generics
                        .first()
                        .map(Self::from_ast_type)
                        .unwrap_or(Self::Vector(ScalarKind::F32, 4));
                    Self::StorageBuffer(Box::new(inner))
                }
                "RWTexture2D" => Self::Texture2D,
                "RWTexture3D" => Self::Texture3D,
                other => Self::Unknown(other.to_string()),
            },
            Type::Unit(_) => Self::Void,
            _ => Self::Unknown(format!("{ty:?}")),
        }
    }

    fn hlsl_name(&self) -> String {
        match self {
            Self::Void => "void".to_string(),
            Self::Scalar(kind) => hlsl_scalar_name(*kind).to_string(),
            Self::Vector(kind, n) => format!("{}{}", hlsl_scalar_name(*kind), n),
            Self::Matrix(rows, cols) => format!("float{}x{}", rows, cols),
            Self::StorageBuffer(inner) => format!("RWStructuredBuffer<{}>", inner.hlsl_name()),
            Self::Texture2D => "Texture2D".to_string(),
            Self::Texture3D => "Texture3D".to_string(),
            Self::TextureCube => "TextureCube".to_string(),
            Self::Sampler => "SamplerState".to_string(),
            Self::Unknown(name) => name.clone(),
        }
    }

    fn wgsl_name(&self) -> String {
        match self {
            Self::Void => "()".to_string(),
            Self::Scalar(kind) => wgsl_scalar_name(*kind).to_string(),
            Self::Vector(kind, n) => format!("vec{}<{}>", n, wgsl_scalar_name(*kind)),
            Self::Matrix(rows, cols) => format!("mat{}x{}<f32>", cols, rows),
            Self::StorageBuffer(inner) => format!("array<{}>", inner.wgsl_name()),
            Self::Texture2D => "texture_2d<f32>".to_string(),
            Self::Texture3D => "texture_3d<f32>".to_string(),
            Self::TextureCube => "texture_cube<f32>".to_string(),
            Self::Sampler => "sampler".to_string(),
            Self::Unknown(name) => name.clone(),
        }
    }

    fn is_boolish(&self) -> bool {
        matches!(self, Self::Scalar(ScalarKind::Bool))
    }

    fn swizzle(&self, field: &str) -> Self {
        let scalar = match self {
            Self::Vector(kind, _) => *kind,
            Self::Scalar(kind) => *kind,
            _ => ScalarKind::F32,
        };
        match field.len() {
            1 => Self::Scalar(scalar),
            2..=4 => Self::Vector(scalar, field.len()),
            _ => Self::Unknown("swizzle".to_string()),
        }
    }

    fn element(&self) -> Self {
        match self {
            Self::StorageBuffer(inner) => (**inner).clone(),
            Self::Vector(kind, _) => Self::Scalar(*kind),
            other => other.clone(),
        }
    }

    fn zero_literal(&self, backend: TextShaderBackend) -> String {
        match self {
            Self::Void => String::new(),
            Self::Scalar(ScalarKind::F32) => "0.0".to_string(),
            Self::Scalar(ScalarKind::I32) => "0".to_string(),
            Self::Scalar(ScalarKind::U32) => match backend {
                TextShaderBackend::Wgsl => "0u".to_string(),
                _ => "0".to_string(),
            },
            Self::Scalar(ScalarKind::Bool) => "false".to_string(),
            Self::Vector(_, n) => {
                let ctor = match backend {
                    TextShaderBackend::Hlsl | TextShaderBackend::Usf => self.hlsl_name(),
                    TextShaderBackend::Wgsl => self.wgsl_name(),
                };
                let scalar_zero = match self {
                    Self::Vector(kind, _) => Self::Scalar(*kind).zero_literal(backend),
                    _ => "0.0".to_string(),
                };
                format!("{ctor}({})", vec![scalar_zero; *n].join(", "))
            }
            Self::Matrix(rows, cols) => {
                let ctor = match backend {
                    TextShaderBackend::Hlsl | TextShaderBackend::Usf => self.hlsl_name(),
                    TextShaderBackend::Wgsl => self.wgsl_name(),
                };
                format!("{ctor}({})", vec!["0.0"; rows * cols].join(", "))
            }
            Self::StorageBuffer(_)
            | Self::Texture2D
            | Self::Texture3D
            | Self::TextureCube
            | Self::Sampler
            | Self::Unknown(_) => "0".to_string(),
        }
    }
}

fn hlsl_scalar_name(kind: ScalarKind) -> &'static str {
    match kind {
        ScalarKind::F32 => "float",
        ScalarKind::I32 => "int",
        ScalarKind::U32 => "uint",
        ScalarKind::Bool => "bool",
    }
}

fn wgsl_scalar_name(kind: ScalarKind) -> &'static str {
    match kind {
        ScalarKind::F32 => "f32",
        ScalarKind::I32 => "i32",
        ScalarKind::U32 => "u32",
        ScalarKind::Bool => "bool",
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ResourceKind {
    LocalSize,
    Uniform,
    StorageBuffer,
    Texture,
    Sampler,
}

pub fn classify_resource(name: &str, ty: &Type) -> ResourceKind {
    if matches!(name, "LOCAL_SIZE_X" | "LOCAL_SIZE_Y" | "LOCAL_SIZE_Z") {
        return ResourceKind::LocalSize;
    }
    match ShaderValueType::from_ast_type(ty) {
        ShaderValueType::StorageBuffer(_) => ResourceKind::StorageBuffer,
        ShaderValueType::Texture2D | ShaderValueType::Texture3D | ShaderValueType::TextureCube => {
            ResourceKind::Texture
        }
        ShaderValueType::Sampler => ResourceKind::Sampler,
        _ => ResourceKind::Uniform,
    }
}

#[derive(Debug, Clone)]
pub struct TypeMapper {
    hlsl: HashMap<&'static str, &'static str>,
    wgsl: HashMap<&'static str, &'static str>,
}

impl TypeMapper {
    pub fn new() -> Self {
        let pairs = [
            ("Float", "float", "f32"),
            ("f32", "float", "f32"),
            ("Int", "int", "i32"),
            ("i32", "int", "i32"),
            ("UInt", "uint", "u32"),
            ("u32", "uint", "u32"),
            ("Bool", "bool", "bool"),
            ("Vec2", "float2", "vec2<f32>"),
            ("Vec3", "float3", "vec3<f32>"),
            ("Vec3A", "float3", "vec3<f32>"),
            ("Vec4", "float4", "vec4<f32>"),
            ("IVec2", "int2", "vec2<i32>"),
            ("IVec3", "int3", "vec3<i32>"),
            ("IVec4", "int4", "vec4<i32>"),
            ("UVec2", "uint2", "vec2<u32>"),
            ("UVec3", "uint3", "vec3<u32>"),
            ("UVec4", "uint4", "vec4<u32>"),
            ("Mat2", "float2x2", "mat2x2<f32>"),
            ("Mat3", "float3x3", "mat3x3<f32>"),
            ("Mat4", "float4x4", "mat4x4<f32>"),
            ("Sampler2D", "Texture2D", "texture_2d<f32>"),
            ("Sampler3D", "Texture3D", "texture_3d<f32>"),
            ("SamplerCube", "TextureCube", "texture_cube<f32>"),
            ("RWBuffer", "RWBuffer", "array<vec4<f32>>"),
            (
                "RWTexture2D",
                "RWTexture2D",
                "texture_storage_2d<rgba8unorm, write>",
            ),
            (
                "RWTexture3D",
                "RWTexture3D",
                "texture_storage_3d<rgba8unorm, write>",
            ),
            ("Void", "void", "()"),
        ];
        Self {
            hlsl: pairs.iter().map(|(k, hlsl, _)| (*k, *hlsl)).collect(),
            wgsl: pairs.iter().map(|(k, _, wgsl)| (*k, *wgsl)).collect(),
        }
    }

    pub fn can_map(&self, kain_type: &str) -> bool {
        self.hlsl.contains_key(kain_type)
    }

    pub fn map_to_hlsl(&self, kain_type: &str) -> Option<String> {
        self.hlsl.get(kain_type).map(|value| (*value).to_string())
    }

    pub fn map_to_wgsl(&self, kain_type: &str) -> Option<String> {
        self.wgsl.get(kain_type).map(|value| (*value).to_string())
    }

    pub fn valid_types(&self) -> Vec<String> {
        let mut types = self
            .hlsl
            .keys()
            .map(|value| (*value).to_string())
            .collect::<Vec<_>>();
        types.sort();
        types
    }
}

impl Default for TypeMapper {
    fn default() -> Self {
        Self::new()
    }
}

pub static TYPE_MAPPER: Lazy<TypeMapper> = Lazy::new(TypeMapper::new);

pub fn map_type_to_hlsl(ty: &Type) -> String {
    ShaderValueType::from_ast_type(ty).hlsl_name()
}

pub fn map_type_to_wgsl(ty: &Type) -> String {
    ShaderValueType::from_ast_type(ty).wgsl_name()
}

pub fn sanitize_identifier(name: &str, backend: TextShaderBackend) -> String {
    let reserved = match backend {
        TextShaderBackend::Hlsl | TextShaderBackend::Usf => &*HLSL_RESERVED,
        TextShaderBackend::Wgsl => &*WGSL_RESERVED,
    };
    let mut out = String::with_capacity(name.len() + 1);
    for (index, ch) in name.chars().enumerate() {
        if (index == 0 && (ch.is_ascii_alphabetic() || ch == '_'))
            || (index > 0 && (ch.is_ascii_alphanumeric() || ch == '_'))
        {
            out.push(ch);
        } else if index == 0 && ch.is_ascii_digit() {
            out.push('_');
            out.push(ch);
        } else {
            out.push('_');
        }
    }
    if out.is_empty() {
        out.push('_');
    }
    if reserved.contains(out.as_str()) {
        out.push('_');
    }
    out
}

fn reserve_wgsl_sampler_binding(
    used_bindings: &mut HashSet<u32>,
    texture_binding: u32,
    span: Span,
) -> KainResult<u32> {
    let mut candidate = texture_binding.checked_add(1).ok_or_else(|| {
        KainError::codegen(
            "WGSL backend cannot reserve a synthetic sampler binding after u32::MAX texture binding",
            span,
        )
    })?;
    while used_bindings.contains(&candidate) {
        candidate = candidate.checked_add(1).ok_or_else(|| {
            KainError::codegen(
                "WGSL backend cannot reserve a free synthetic sampler binding",
                span,
            )
        })?;
    }
    used_bindings.insert(candidate);
    Ok(candidate)
}

static HLSL_RESERVED: Lazy<HashSet<&'static str>> = Lazy::new(|| {
    [
        "asm",
        "bool",
        "break",
        "case",
        "cbuffer",
        "class",
        "compile",
        "const",
        "continue",
        "default",
        "discard",
        "do",
        "double",
        "else",
        "export",
        "extern",
        "false",
        "float",
        "for",
        "groupshared",
        "half",
        "if",
        "in",
        "inline",
        "int",
        "line",
        "matrix",
        "namespace",
        "out",
        "pass",
        "return",
        "sampler",
        "shared",
        "static",
        "string",
        "struct",
        "switch",
        "technique",
        "texture",
        "true",
        "typedef",
        "uniform",
        "uint",
        "vector",
        "void",
        "volatile",
        "while",
    ]
    .into_iter()
    .collect()
});

static WGSL_RESERVED: Lazy<HashSet<&'static str>> = Lazy::new(|| {
    [
        "alias",
        "array",
        "atomic",
        "bitcast",
        "bool",
        "break",
        "case",
        "const",
        "const_assert",
        "continue",
        "continuing",
        "default",
        "diagnostic",
        "discard",
        "else",
        "enable",
        "false",
        "fn",
        "for",
        "if",
        "let",
        "loop",
        "mat2x2",
        "mat3x3",
        "mat4x4",
        "override",
        "return",
        "struct",
        "switch",
        "true",
        "var",
        "vec2",
        "vec3",
        "vec4",
        "while",
    ]
    .into_iter()
    .collect()
});

#[derive(Clone, Debug)]
struct VarInfo {
    name: String,
    ty: ShaderValueType,
}

#[derive(Clone, Debug)]
struct TextContext {
    backend: TextShaderBackend,
    stage: ShaderStage,
    shader_name: String,
    output_type: ShaderValueType,
    vars: HashMap<String, VarInfo>,
    indent_level: usize,
    temp_id: usize,
}

impl TextContext {
    fn new(backend: TextShaderBackend, shader: &TypedShader) -> Self {
        let mut vars = HashMap::new();
        for uniform in &shader.ast.uniforms {
            if classify_resource(&uniform.name, &uniform.ty) == ResourceKind::LocalSize {
                continue;
            }
            vars.insert(
                uniform.name.clone(),
                VarInfo {
                    name: sanitize_identifier(&uniform.name, backend),
                    ty: ShaderValueType::from_ast_type(&uniform.ty),
                },
            );
        }
        Self {
            backend,
            stage: shader.ast.stage,
            shader_name: sanitize_identifier(&shader.ast.name, backend),
            output_type: ShaderValueType::from_ast_type(&shader.ast.outputs),
            vars,
            indent_level: 0,
            temp_id: 0,
        }
    }

    fn indent(&self) -> String {
        "    ".repeat(self.indent_level)
    }

    fn push_indent(&mut self) {
        self.indent_level += 1;
    }

    fn pop_indent(&mut self) {
        self.indent_level = self.indent_level.saturating_sub(1);
    }

    fn next_temp(&mut self, prefix: &str) -> String {
        let id = self.temp_id;
        self.temp_id += 1;
        format!("_{prefix}_{id}")
    }

    fn declare_var(&mut self, source_name: &str, output_name: String, ty: ShaderValueType) {
        self.vars.insert(
            source_name.to_string(),
            VarInfo {
                name: output_name,
                ty,
            },
        );
    }

    fn type_name(&self, ty: &ShaderValueType) -> String {
        match self.backend {
            TextShaderBackend::Hlsl | TextShaderBackend::Usf => ty.hlsl_name(),
            TextShaderBackend::Wgsl => ty.wgsl_name(),
        }
    }

    fn codegen_error(&self, message: impl Into<String>, span: Span) -> KainError {
        KainError::codegen(
            format!(
                "{} text shader backend does not support {}",
                self.backend.label(),
                message.into()
            ),
            span,
        )
    }
}

pub mod hlsl {
    use super::*;

    pub fn generate(program: &TypedProgram) -> KainResult<String> {
        let mut output = String::new();
        output.push_str("// Generated by KAIN Compiler\n");
        output.push_str("// Derived HLSL output from the KAIN shader artifact pipeline\n\n");
        let mut emitted = 0usize;
        for item in &program.items {
            if let TypedItem::Shader(shader) = item {
                emitted += 1;
                output.push_str(&emit_shader(shader)?);
                output.push('\n');
            }
        }
        if emitted == 0 {
            return Err(KainError::codegen(
                "HLSL backend emitted no shaders: the typed program contains no shader items",
                Span::default(),
            ));
        }
        Ok(output)
    }

    fn emit_shader(shader: &TypedShader) -> KainResult<String> {
        let mut ctx = TextContext::new(TextShaderBackend::Hlsl, shader);
        let mut output = String::new();
        output.push_str(&format!("// Shader: {}\n", shader.ast.name));
        output.push_str(&emit_hlsl_resources(shader)?);
        match shader.ast.stage {
            ShaderStage::Compute => emit_compute_shader(&mut ctx, shader, &mut output)?,
            ShaderStage::Vertex => emit_vertex_shader(&mut ctx, shader, &mut output)?,
            ShaderStage::Fragment | ShaderStage::Surface => {
                emit_fragment_shader(&mut ctx, shader, &mut output)?
            }
            ShaderStage::Mesh => emit_mesh_shader(&mut ctx, shader, &mut output)?,
            ShaderStage::Task => emit_task_shader(&mut ctx, shader, &mut output)?,
            ShaderStage::RayGen => emit_raygen_shader(&mut ctx, shader, &mut output)?,
            ShaderStage::AnyHit => emit_anyhit_shader(&mut ctx, shader, &mut output)?,
            ShaderStage::ClosestHit => emit_closesthit_shader(&mut ctx, shader, &mut output)?,
            ShaderStage::Miss => emit_miss_shader(&mut ctx, shader, &mut output)?,
            ShaderStage::Intersection => emit_intersection_shader(&mut ctx, shader, &mut output)?,
            ShaderStage::Callable => emit_callable_shader(&mut ctx, shader, &mut output)?,
        }
        Ok(output)
    }

    fn emit_hlsl_resources(shader: &TypedShader) -> KainResult<String> {
        let mut output = String::new();
        let mut uniforms = Vec::new();
        let mut textures = Vec::new();
        let mut samplers = Vec::new();
        let mut buffers = Vec::new();
        for uniform in &shader.ast.uniforms {
            let name = sanitize_identifier(&uniform.name, TextShaderBackend::Hlsl);
            match classify_resource(&uniform.name, &uniform.ty) {
                ResourceKind::LocalSize => {}
                ResourceKind::Uniform => uniforms.push((name, map_type_to_hlsl(&uniform.ty))),
                ResourceKind::Texture => {
                    textures.push((name, map_type_to_hlsl(&uniform.ty), uniform.binding))
                }
                ResourceKind::Sampler => samplers.push((name, uniform.binding)),
                ResourceKind::StorageBuffer => {
                    let ty = ShaderValueType::from_ast_type(&uniform.ty).hlsl_name();
                    buffers.push((name, ty, uniform.binding));
                }
            }
        }
        if !uniforms.is_empty() {
            output.push_str(&format!(
                "cbuffer {}Params : register(b0)\n{{\n",
                sanitize_identifier(&shader.ast.name, TextShaderBackend::Hlsl)
            ));
            for (name, ty) in uniforms {
                output.push_str(&format!("    {ty} {name};\n"));
            }
            output.push_str("};\n\n");
        }
        for (name, ty, binding) in textures {
            output.push_str(&format!("{ty} {name} : register(t{binding});\n"));
            if !samplers
                .iter()
                .any(|(sampler, _)| sampler == &format!("{name}_sampler"))
            {
                output.push_str(&format!(
                    "SamplerState {name}_sampler : register(s{binding});\n"
                ));
            }
        }
        for (name, binding) in samplers {
            output.push_str(&format!("SamplerState {name} : register(s{binding});\n"));
        }
        for (name, ty, binding) in buffers {
            output.push_str(&format!("{ty} {name} : register(u{binding});\n"));
        }
        if !output.is_empty() {
            output.push('\n');
        }
        Ok(output)
    }

    fn emit_compute_shader(
        ctx: &mut TextContext,
        shader: &TypedShader,
        output: &mut String,
    ) -> KainResult<()> {
        let [x, y, z] = shader.ast.workgroup_size.unwrap_or(DEFAULT_WORKGROUP_SIZE);
        output.push_str(&format!("[numthreads({x}, {y}, {z})]\n"));
        output.push_str(&format!(
            "void {}(uint3 dispatch_thread_id : SV_DispatchThreadID,\n",
            ctx.shader_name
        ));
        output.push_str("          uint3 group_thread_id : SV_GroupThreadID,\n");
        output.push_str("          uint3 workgroup_id : SV_GroupID,\n");
        output.push_str("          uint group_index : SV_GroupIndex)\n{\n");
        ctx.push_indent();
        seed_compute_builtins(ctx, shader, output)?;
        output.push_str(&emit_block(ctx, &shader.ast.body)?);
        ctx.pop_indent();
        output.push_str("}\n");
        Ok(())
    }

    fn emit_vertex_shader(
        ctx: &mut TextContext,
        shader: &TypedShader,
        output: &mut String,
    ) -> KainResult<()> {
        let input_struct = format!("{}Input", ctx.shader_name);
        let output_struct = format!("{}Output", ctx.shader_name);
        output.push_str(&format!("struct {input_struct}\n{{\n"));
        for (index, param) in shader.ast.inputs.iter().enumerate() {
            let name = sanitize_identifier(&param.name, TextShaderBackend::Hlsl);
            let semantic = if param.name == "position" {
                "POSITION".to_string()
            } else if param.name == "normal" {
                "NORMAL".to_string()
            } else if param.name == "color" {
                "COLOR".to_string()
            } else {
                format!("TEXCOORD{index}")
            };
            output.push_str(&format!(
                "    {} {name} : {semantic};\n",
                map_type_to_hlsl(&param.ty)
            ));
        }
        output.push_str("};\n\n");
        output.push_str(&format!("struct {output_struct}\n{{\n"));
        output.push_str("    float4 position : SV_Position;\n");
        output.push_str("};\n\n");
        output.push_str(&format!(
            "{output_struct} {}({input_struct} input)\n{{\n",
            ctx.shader_name
        ));
        ctx.push_indent();
        for param in &shader.ast.inputs {
            let name = sanitize_identifier(&param.name, TextShaderBackend::Hlsl);
            ctx.declare_var(
                &param.name,
                format!("input.{name}"),
                ShaderValueType::from_ast_type(&param.ty),
            );
        }
        output.push_str(&emit_block(ctx, &shader.ast.body)?);
        if !block_has_return(&shader.ast.body) {
            output.push_str(&format!("{}{} _result;\n", ctx.indent(), output_struct));
            output.push_str(&format!(
                "{}_result.position = float4(0.0, 0.0, 0.0, 1.0);\n",
                ctx.indent()
            ));
            output.push_str(&format!("{}return _result;\n", ctx.indent()));
        }
        ctx.pop_indent();
        output.push_str("}\n");
        Ok(())
    }

    fn emit_fragment_shader(
        ctx: &mut TextContext,
        shader: &TypedShader,
        output: &mut String,
    ) -> KainResult<()> {
        let input_struct = format!("{}Input", ctx.shader_name);
        let output_struct = format!("{}Output", ctx.shader_name);
        output.push_str(&format!("struct {input_struct}\n{{\n"));
        for (index, param) in shader.ast.inputs.iter().enumerate() {
            let name = sanitize_identifier(&param.name, TextShaderBackend::Hlsl);
            output.push_str(&format!(
                "    {} {name} : TEXCOORD{index};\n",
                map_type_to_hlsl(&param.ty)
            ));
        }
        output.push_str("};\n\n");
        output.push_str(&format!("struct {output_struct}\n{{\n"));
        output.push_str(&format!(
            "    {} color : SV_Target0;\n",
            map_type_to_hlsl(&shader.ast.outputs)
        ));
        output.push_str("};\n\n");
        output.push_str(&format!(
            "{output_struct} {}({input_struct} input)\n{{\n",
            ctx.shader_name
        ));
        ctx.push_indent();
        for param in &shader.ast.inputs {
            let name = sanitize_identifier(&param.name, TextShaderBackend::Hlsl);
            ctx.declare_var(
                &param.name,
                format!("input.{name}"),
                ShaderValueType::from_ast_type(&param.ty),
            );
        }
        output.push_str(&emit_block(ctx, &shader.ast.body)?);
        if !block_has_return(&shader.ast.body) {
            output.push_str(&format!("{}{} _result;\n", ctx.indent(), output_struct));
            output.push_str(&format!(
                "{}_result.color = {};\n",
                ctx.indent(),
                ctx.output_type.zero_literal(ctx.backend)
            ));
            output.push_str(&format!("{}return _result;\n", ctx.indent()));
        }
        ctx.pop_indent();
        output.push_str("}\n");
        Ok(())
    }

    // ── Mesh / Task / Ray tracing shader stubs (HLSL) ──────────────────

    fn emit_mesh_shader(
        ctx: &mut TextContext,
        shader: &TypedShader,
        output: &mut String,
    ) -> KainResult<()> {
        let [x, y, z] = shader.ast.workgroup_size.unwrap_or(DEFAULT_WORKGROUP_SIZE);
        output.push_str(&format!("[numthreads({x}, {y}, {z})]\n"));
        output.push_str("[outputtopology(\"triangle\")]\n");
        output.push_str(&format!("void {}(\n", ctx.shader_name));
        output.push_str("    out indices uint3 primitive : SV_PrimitiveID,\n");
        output.push_str("    out vertices float4 position : SV_Position)\n{\n");
        ctx.push_indent();
        seed_compute_builtins(ctx, shader, output)?;
        output.push_str(&emit_block(ctx, &shader.ast.body)?);
        ctx.pop_indent();
        output.push_str("}\n");
        Ok(())
    }

    fn emit_task_shader(
        ctx: &mut TextContext,
        shader: &TypedShader,
        output: &mut String,
    ) -> KainResult<()> {
        let [x, y, z] = shader.ast.workgroup_size.unwrap_or(DEFAULT_WORKGROUP_SIZE);
        output.push_str(&format!("[numthreads({x}, {y}, {z})]\n"));
        output.push_str(&format!("void {}(\n", ctx.shader_name));
        output.push_str("    out indices uint3 group_id : SV_GroupID)\n{\n");
        ctx.push_indent();
        seed_compute_builtins(ctx, shader, output)?;
        output.push_str(&emit_block(ctx, &shader.ast.body)?);
        ctx.pop_indent();
        output.push_str("}\n");
        Ok(())
    }

    fn emit_raygen_shader(
        ctx: &mut TextContext,
        _shader: &TypedShader,
        output: &mut String,
    ) -> KainResult<()> {
        output.push_str("[shader(\"raygeneration\")]\n");
        output.push_str(&format!("void {}()\n{{\n", ctx.shader_name));
        ctx.push_indent();
        output.push_str(&format!("{}// RayGeneration shader entry — payload access via DispatchRaysIndex()\n", ctx.indent()));
        ctx.pop_indent();
        output.push_str("}\n");
        Ok(())
    }

    fn emit_closesthit_shader(
        ctx: &mut TextContext,
        _shader: &TypedShader,
        output: &mut String,
    ) -> KainResult<()> {
        output.push_str("[shader(\"closesthit\")]\n");
        output.push_str(&format!("void {}(inout RayPayload payload)\n{{\n", ctx.shader_name));
        ctx.push_indent();
        output.push_str(&format!("{}// ClosestHit shader entry\n", ctx.indent()));
        ctx.pop_indent();
        output.push_str("}\n");
        Ok(())
    }

    fn emit_anyhit_shader(
        ctx: &mut TextContext,
        _shader: &TypedShader,
        output: &mut String,
    ) -> KainResult<()> {
        output.push_str("[shader(\"anyhit\")]\n");
        output.push_str(&format!("void {}(inout RayPayload payload)\n{{\n", ctx.shader_name));
        ctx.push_indent();
        output.push_str(&format!("{}// AnyHit shader entry\n", ctx.indent()));
        ctx.pop_indent();
        output.push_str("}\n");
        Ok(())
    }

    fn emit_miss_shader(
        ctx: &mut TextContext,
        _shader: &TypedShader,
        output: &mut String,
    ) -> KainResult<()> {
        output.push_str("[shader(\"miss\")]\n");
        output.push_str(&format!("void {}(inout RayPayload payload)\n{{\n", ctx.shader_name));
        ctx.push_indent();
        output.push_str(&format!("{}// Miss shader entry\n", ctx.indent()));
        ctx.pop_indent();
        output.push_str("}\n");
        Ok(())
    }

    fn emit_intersection_shader(
        ctx: &mut TextContext,
        _shader: &TypedShader,
        output: &mut String,
    ) -> KainResult<()> {
        output.push_str("[shader(\"intersection\")]\n");
        output.push_str(&format!("void {}()\n{{\n", ctx.shader_name));
        ctx.push_indent();
        output.push_str(&format!("{}// Intersection shader entry\n", ctx.indent()));
        ctx.pop_indent();
        output.push_str("}\n");
        Ok(())
    }

    fn emit_callable_shader(
        ctx: &mut TextContext,
        _shader: &TypedShader,
        output: &mut String,
    ) -> KainResult<()> {
        output.push_str("[shader(\"callable\")]\n");
        output.push_str(&format!("void {}(inout CallableData payload)\n{{\n", ctx.shader_name));
        ctx.push_indent();
        output.push_str(&format!("{}// Callable shader entry\n", ctx.indent()));
        ctx.pop_indent();
        output.push_str("}\n");
        Ok(())
    }
}

pub mod wgsl {
    use super::*;

    pub fn generate(program: &TypedProgram) -> KainResult<String> {
        let mut output = String::new();
        output.push_str("// Generated by KAIN Compiler\n");
        output.push_str("// Direct WGSL output from the KAIN shader artifact pipeline\n\n");
        let mut emitted = 0usize;
        for item in &program.items {
            if let TypedItem::Shader(shader) = item {
                emitted += 1;
                output.push_str(&emit_shader(shader)?);
                output.push('\n');
            }
        }
        if emitted == 0 {
            return Err(KainError::codegen(
                "WGSL backend emitted no shaders: the typed program contains no shader items",
                Span::default(),
            ));
        }
        Ok(output)
    }

    fn emit_shader(shader: &TypedShader) -> KainResult<String> {
        let mut ctx = TextContext::new(TextShaderBackend::Wgsl, shader);
        let mut output = String::new();
        output.push_str(&format!("// Shader: {}\n", shader.ast.name));
        output.push_str(&emit_wgsl_resources(shader)?);
        match shader.ast.stage {
            ShaderStage::Compute => emit_compute_shader(&mut ctx, shader, &mut output)?,
            ShaderStage::Vertex => emit_vertex_shader(&mut ctx, shader, &mut output)?,
            ShaderStage::Fragment | ShaderStage::Surface => {
                emit_fragment_shader(&mut ctx, shader, &mut output)?
            }
            ShaderStage::Mesh => emit_mesh_shader_wgsl(&mut ctx, shader, &mut output)?,
            ShaderStage::Task => emit_task_shader_wgsl(&mut ctx, shader, &mut output)?,
            ShaderStage::RayGen => emit_raygen_shader_wgsl(&mut ctx, shader, &mut output)?,
            ShaderStage::AnyHit => emit_anyhit_shader_wgsl(&mut ctx, shader, &mut output)?,
            ShaderStage::ClosestHit => emit_closesthit_shader_wgsl(&mut ctx, shader, &mut output)?,
            ShaderStage::Miss => emit_miss_shader_wgsl(&mut ctx, shader, &mut output)?,
            ShaderStage::Intersection => emit_intersection_shader_wgsl(&mut ctx, shader, &mut output)?,
            ShaderStage::Callable => emit_callable_shader_wgsl(&mut ctx, shader, &mut output)?,
        }
        Ok(output)
    }

    fn emit_wgsl_resources(shader: &TypedShader) -> KainResult<String> {
        let mut output = String::new();
        let mut used_bindings = HashSet::new();
        let mut explicit_samplers = HashSet::new();
        for uniform in &shader.ast.uniforms {
            if classify_resource(&uniform.name, &uniform.ty) == ResourceKind::LocalSize {
                continue;
            }
            if !used_bindings.insert(uniform.binding) {
                return Err(KainError::codegen(
                    format!(
                        "WGSL backend found duplicate resource binding @{} in shader '{}'",
                        uniform.binding, shader.ast.name
                    ),
                    uniform.span,
                ));
            }
            if classify_resource(&uniform.name, &uniform.ty) == ResourceKind::Sampler {
                explicit_samplers
                    .insert(sanitize_identifier(&uniform.name, TextShaderBackend::Wgsl));
            }
        }
        for uniform in &shader.ast.uniforms {
            if classify_resource(&uniform.name, &uniform.ty) == ResourceKind::LocalSize {
                continue;
            }
            let name = sanitize_identifier(&uniform.name, TextShaderBackend::Wgsl);
            match classify_resource(&uniform.name, &uniform.ty) {
                ResourceKind::LocalSize => {}
                ResourceKind::Uniform => output.push_str(&format!(
                    "@group(0) @binding({}) var<uniform> {}: {};\n",
                    uniform.binding,
                    name,
                    map_type_to_wgsl(&uniform.ty)
                )),
                ResourceKind::StorageBuffer => {
                    let value_ty = ShaderValueType::from_ast_type(&uniform.ty);
                    output.push_str(&format!(
                        "@group(0) @binding({}) var<storage, read_write> {}: {};\n",
                        uniform.binding,
                        name,
                        value_ty.wgsl_name()
                    ));
                }
                ResourceKind::Texture => {
                    output.push_str(&format!(
                        "@group(0) @binding({}) var {}: {};\n",
                        uniform.binding,
                        name,
                        map_type_to_wgsl(&uniform.ty)
                    ));
                    let sampler_name = format!("{name}_sampler");
                    if !explicit_samplers.contains(&sampler_name) {
                        let sampler_binding = reserve_wgsl_sampler_binding(
                            &mut used_bindings,
                            uniform.binding,
                            uniform.span,
                        )?;
                        output.push_str(&format!(
                            "@group(0) @binding({sampler_binding}) var {sampler_name}: sampler;\n"
                        ));
                    }
                }
                ResourceKind::Sampler => output.push_str(&format!(
                    "@group(0) @binding({}) var {}: sampler;\n",
                    uniform.binding, name
                )),
            }
        }
        if !output.is_empty() {
            output.push('\n');
        }
        Ok(output)
    }

    fn emit_compute_shader(
        ctx: &mut TextContext,
        shader: &TypedShader,
        output: &mut String,
    ) -> KainResult<()> {
        let [x, y, z] = shader.ast.workgroup_size.unwrap_or(DEFAULT_WORKGROUP_SIZE);
        output.push_str(&format!(
            "@compute @workgroup_size({x}, {y}, {z})\nfn {}(\n",
            ctx.shader_name
        ));
        output.push_str("    @builtin(global_invocation_id) dispatch_thread_id: vec3<u32>,\n");
        output.push_str("    @builtin(local_invocation_id) group_thread_id: vec3<u32>,\n");
        output.push_str("    @builtin(workgroup_id) workgroup_id: vec3<u32>,\n");
        output.push_str("    @builtin(local_invocation_index) group_index: u32\n");
        output.push_str(") {\n");
        ctx.push_indent();
        seed_compute_builtins(ctx, shader, output)?;
        output.push_str(&emit_block(ctx, &shader.ast.body)?);
        ctx.pop_indent();
        output.push_str("}\n");
        Ok(())
    }

    fn emit_vertex_shader(
        ctx: &mut TextContext,
        shader: &TypedShader,
        output: &mut String,
    ) -> KainResult<()> {
        let output_struct = format!("{}Output", ctx.shader_name);
        output.push_str(&format!("struct {output_struct} {{\n"));
        output.push_str("    @builtin(position) position: vec4<f32>,\n");
        output.push_str("};\n\n");
        output.push_str(&format!("@vertex\nfn {}(\n", ctx.shader_name));
        for (index, param) in shader.ast.inputs.iter().enumerate() {
            let comma = if index + 1 == shader.ast.inputs.len() {
                ""
            } else {
                ","
            };
            output.push_str(&format!(
                "    @location({index}) {}: {}{comma}\n",
                sanitize_identifier(&param.name, TextShaderBackend::Wgsl),
                map_type_to_wgsl(&param.ty)
            ));
        }
        output.push_str(&format!(") -> {output_struct} {{\n"));
        ctx.push_indent();
        for param in &shader.ast.inputs {
            ctx.declare_var(
                &param.name,
                sanitize_identifier(&param.name, TextShaderBackend::Wgsl),
                ShaderValueType::from_ast_type(&param.ty),
            );
        }
        output.push_str(&emit_block(ctx, &shader.ast.body)?);
        if !block_has_return(&shader.ast.body) {
            output.push_str(&format!("{}var _result: {output_struct};\n", ctx.indent()));
            output.push_str(&format!(
                "{}_result.position = vec4<f32>(0.0, 0.0, 0.0, 1.0);\n",
                ctx.indent()
            ));
            output.push_str(&format!("{}return _result;\n", ctx.indent()));
        }
        ctx.pop_indent();
        output.push_str("}\n");
        Ok(())
    }

    fn emit_fragment_shader(
        ctx: &mut TextContext,
        shader: &TypedShader,
        output: &mut String,
    ) -> KainResult<()> {
        output.push_str(&format!("@fragment\nfn {}(\n", ctx.shader_name));
        for (index, param) in shader.ast.inputs.iter().enumerate() {
            let comma = if index + 1 == shader.ast.inputs.len() {
                ""
            } else {
                ","
            };
            output.push_str(&format!(
                "    @location({index}) {}: {}{comma}\n",
                sanitize_identifier(&param.name, TextShaderBackend::Wgsl),
                map_type_to_wgsl(&param.ty)
            ));
        }
        output.push_str(&format!(
            ") -> @location(0) {} {{\n",
            map_type_to_wgsl(&shader.ast.outputs)
        ));
        ctx.push_indent();
        for param in &shader.ast.inputs {
            ctx.declare_var(
                &param.name,
                sanitize_identifier(&param.name, TextShaderBackend::Wgsl),
                ShaderValueType::from_ast_type(&param.ty),
            );
        }
        output.push_str(&emit_block(ctx, &shader.ast.body)?);
        if !block_has_return(&shader.ast.body) {
            output.push_str(&format!(
                "{}return {};\n",
                ctx.indent(),
                ctx.output_type.zero_literal(ctx.backend)
            ));
        }
        ctx.pop_indent();
        output.push_str("}\n");
        Ok(())
    }

    // ── Mesh / Task / Ray tracing shader stubs (WGSL) ──────────────────

    fn emit_mesh_shader_wgsl(
        ctx: &mut TextContext,
        shader: &TypedShader,
        output: &mut String,
    ) -> KainResult<()> {
        let [x, y, z] = shader.ast.workgroup_size.unwrap_or(DEFAULT_WORKGROUP_SIZE);
        output.push_str(&format!("@mesh @workgroup_size({x}, {y}, {z})\n"));
        output.push_str(&format!("fn {}(\n", ctx.shader_name));
        output.push_str("    @builtin(position) position: vec4<f32>,\n");
        output.push_str("    @builtin(primitive_index) primitive_index: u32\n");
        output.push_str(") {\n");
        ctx.push_indent();
        seed_compute_builtins(ctx, shader, output)?;
        output.push_str(&emit_block(ctx, &shader.ast.body)?);
        ctx.pop_indent();
        output.push_str("}\n");
        Ok(())
    }

    fn emit_task_shader_wgsl(
        ctx: &mut TextContext,
        shader: &TypedShader,
        output: &mut String,
    ) -> KainResult<()> {
        let [x, y, z] = shader.ast.workgroup_size.unwrap_or(DEFAULT_WORKGROUP_SIZE);
        output.push_str(&format!("@task @workgroup_size({x}, {y}, {z})\n"));
        output.push_str(&format!("fn {}() {{\n", ctx.shader_name));
        ctx.push_indent();
        seed_compute_builtins(ctx, shader, output)?;
        output.push_str(&emit_block(ctx, &shader.ast.body)?);
        ctx.pop_indent();
        output.push_str("}\n");
        Ok(())
    }

    fn emit_raygen_shader_wgsl(
        ctx: &mut TextContext,
        _shader: &TypedShader,
        output: &mut String,
    ) -> KainResult<()> {
        output.push_str(&format!("@raygen fn {}() {{\n", ctx.shader_name));
        ctx.push_indent();
        output.push_str(&format!("{}// RayGeneration shader entry\n", ctx.indent()));
        ctx.pop_indent();
        output.push_str("}\n");
        Ok(())
    }

    fn emit_closesthit_shader_wgsl(
        ctx: &mut TextContext,
        _shader: &TypedShader,
        output: &mut String,
    ) -> KainResult<()> {
        output.push_str(&format!("@closesthit fn {}() {{\n", ctx.shader_name));
        ctx.push_indent();
        output.push_str(&format!("{}// ClosestHit shader entry\n", ctx.indent()));
        ctx.pop_indent();
        output.push_str("}\n");
        Ok(())
    }

    fn emit_anyhit_shader_wgsl(
        ctx: &mut TextContext,
        _shader: &TypedShader,
        output: &mut String,
    ) -> KainResult<()> {
        output.push_str(&format!("@anyhit fn {}() {{\n", ctx.shader_name));
        ctx.push_indent();
        output.push_str(&format!("{}// AnyHit shader entry\n", ctx.indent()));
        ctx.pop_indent();
        output.push_str("}\n");
        Ok(())
    }

    fn emit_miss_shader_wgsl(
        ctx: &mut TextContext,
        _shader: &TypedShader,
        output: &mut String,
    ) -> KainResult<()> {
        output.push_str(&format!("@miss fn {}() {{\n", ctx.shader_name));
        ctx.push_indent();
        output.push_str(&format!("{}// Miss shader entry\n", ctx.indent()));
        ctx.pop_indent();
        output.push_str("}\n");
        Ok(())
    }

    fn emit_intersection_shader_wgsl(
        ctx: &mut TextContext,
        _shader: &TypedShader,
        output: &mut String,
    ) -> KainResult<()> {
        output.push_str(&format!("@intersection fn {}() {{\n", ctx.shader_name));
        ctx.push_indent();
        output.push_str(&format!("{}// Intersection shader entry\n", ctx.indent()));
        ctx.pop_indent();
        output.push_str("}\n");
        Ok(())
    }

    fn emit_callable_shader_wgsl(
        ctx: &mut TextContext,
        _shader: &TypedShader,
        output: &mut String,
    ) -> KainResult<()> {
        output.push_str(&format!("@callable fn {}() {{\n", ctx.shader_name));
        ctx.push_indent();
        output.push_str(&format!("{}// Callable shader entry\n", ctx.indent()));
        ctx.pop_indent();
        output.push_str("}\n");
        Ok(())
    }
}

fn seed_compute_builtins(
    ctx: &mut TextContext,
    shader: &TypedShader,
    output: &mut String,
) -> KainResult<()> {
    let uvec3 = ShaderValueType::Vector(ScalarKind::U32, 3);
    let u32_ty = ShaderValueType::Scalar(ScalarKind::U32);
    for (source, output_name, ty) in [
        ("dispatch_thread_id", "dispatch_thread_id", uvec3.clone()),
        ("global_id", "dispatch_thread_id", uvec3.clone()),
        ("group_thread_id", "group_thread_id", uvec3.clone()),
        ("local_invocation_id", "group_thread_id", uvec3.clone()),
        ("workgroup_id", "workgroup_id", uvec3.clone()),
        ("group_id", "workgroup_id", uvec3.clone()),
        ("group_index", "group_index", u32_ty.clone()),
        ("local_invocation_index", "group_index", u32_ty.clone()),
    ] {
        ctx.declare_var(source, output_name.to_string(), ty);
    }
    for param in &shader.ast.inputs {
        let param_ty = ShaderValueType::from_ast_type(&param.ty);
        let name = sanitize_identifier(&param.name, ctx.backend);
        let source = match param_ty {
            ShaderValueType::Vector(ScalarKind::U32, 3) => "dispatch_thread_id",
            ShaderValueType::Scalar(ScalarKind::U32) => "group_index",
            _ => {
                return Err(ctx.codegen_error(
                    format!(
                        "compute input '{}' with type {}; use UVec3 for dispatch ids or UInt for group index",
                        param.name,
                        ctx.type_name(&param_ty)
                    ),
                    param.span,
                ))
            }
        };
        if name != source {
            match ctx.backend {
                TextShaderBackend::Hlsl | TextShaderBackend::Usf => output.push_str(&format!(
                    "{}{} {} = {};\n",
                    ctx.indent(),
                    ctx.type_name(&param_ty),
                    name,
                    source
                )),
                TextShaderBackend::Wgsl => output.push_str(&format!(
                    "{}let {}: {} = {};\n",
                    ctx.indent(),
                    name,
                    ctx.type_name(&param_ty),
                    source
                )),
            }
        }
        ctx.declare_var(&param.name, name, param_ty);
    }
    Ok(())
}

fn emit_block(ctx: &mut TextContext, block: &Block) -> KainResult<String> {
    let mut output = String::new();
    for stmt in &block.stmts {
        output.push_str(&emit_stmt(ctx, stmt)?);
    }
    Ok(output)
}

fn emit_stmt(ctx: &mut TextContext, stmt: &Stmt) -> KainResult<String> {
    match stmt {
        Stmt::Subgroup { .. } => Ok(String::new()),
        Stmt::Let {
            pattern,
            ty,
            value,
            span,
            ..
        } => emit_let(ctx, pattern, ty.as_ref(), value.as_ref(), *span),
        Stmt::Expr(expr) => {
            if let Expr::If {
                condition,
                then_branch,
                else_branch,
                ..
            } = expr
            {
                emit_if_statement(ctx, condition, then_branch, else_branch.as_deref())
            } else {
                let (code, _) = emit_expr(ctx, expr)?;
                Ok(format!("{}{};\n", ctx.indent(), code))
            }
        }
        Stmt::Return(expr, span) => emit_return(ctx, expr.as_ref(), *span),
        Stmt::While {
            condition, body, ..
        } => {
            let (condition, _) = emit_expr(ctx, condition)?;
            let mut output = String::new();
            output.push_str(&format!("{}while ({condition}) {{\n", ctx.indent()));
            ctx.push_indent();
            output.push_str(&emit_block(ctx, body)?);
            ctx.pop_indent();
            output.push_str(&format!("{}}}\n", ctx.indent()));
            Ok(output)
        }
        Stmt::Break(_, _) => Ok(format!("{}break;\n", ctx.indent())),
        Stmt::Continue(_) => Ok(format!("{}continue;\n", ctx.indent())),
        Stmt::For { span, .. } => Err(ctx.codegen_error(
            "for loops in shaders; use while with explicit bounds",
            *span,
        )),
        Stmt::Fanout { span, .. } => Err(ctx.codegen_error("fanout in shaders", *span)),
        Stmt::Loop { span, .. } => Err(ctx.codegen_error(
            "loop in text shaders; use while with explicit bounds",
            *span,
        )),
        Stmt::Dispatch { span, .. } => {
            Err(ctx.codegen_error("host dispatch statements inside shaders", *span))
        }
        Stmt::Defer { span, .. } => Err(ctx.codegen_error("defer inside shaders", *span)),
        Stmt::Item(_) => Ok(String::new()),
    }
}

fn emit_let(
    ctx: &mut TextContext,
    pattern: &Pattern,
    declared_ty: Option<&Type>,
    value: Option<&Expr>,
    span: Span,
) -> KainResult<String> {
    let Pattern::Binding { name, .. } = pattern else {
        return Err(ctx.codegen_error("destructuring let patterns in shaders", span));
    };
    let output_name = sanitize_identifier(name, ctx.backend);
    let (value_code, inferred_ty) = if let Some(value) = value {
        emit_expr(ctx, value)?
    } else {
        (
            String::new(),
            ShaderValueType::Unknown("inferred".to_string()),
        )
    };
    let ty = declared_ty
        .map(ShaderValueType::from_ast_type)
        .unwrap_or(inferred_ty);
    let mut output = String::new();
    match ctx.backend {
        TextShaderBackend::Hlsl | TextShaderBackend::Usf => {
            if value.is_some() {
                output.push_str(&format!(
                    "{}{} {} = {};\n",
                    ctx.indent(),
                    ctx.type_name(&ty),
                    output_name,
                    value_code
                ));
            } else {
                output.push_str(&format!(
                    "{}{} {};\n",
                    ctx.indent(),
                    ctx.type_name(&ty),
                    output_name
                ));
            }
        }
        TextShaderBackend::Wgsl => {
            if value.is_some() {
                output.push_str(&format!(
                    "{}var {}: {} = {};\n",
                    ctx.indent(),
                    output_name,
                    ctx.type_name(&ty),
                    value_code
                ));
            } else {
                output.push_str(&format!(
                    "{}var {}: {};\n",
                    ctx.indent(),
                    output_name,
                    ctx.type_name(&ty)
                ));
            }
        }
    }
    ctx.declare_var(name, output_name, ty);
    Ok(output)
}

fn emit_return(ctx: &mut TextContext, expr: Option<&Expr>, span: Span) -> KainResult<String> {
    let mut output = String::new();
    match ctx.stage {
        ShaderStage::Compute => {
            if let Some(expr) = expr {
                let (code, ty) = emit_expr(ctx, expr)?;
                let temp = ctx.next_temp("return_value");
                match ctx.backend {
                    TextShaderBackend::Hlsl | TextShaderBackend::Usf => output.push_str(&format!(
                        "{}{} {} = {};\n",
                        ctx.indent(),
                        ctx.type_name(&ty),
                        temp,
                        code
                    )),
                    TextShaderBackend::Wgsl => output.push_str(&format!(
                        "{}let {}: {} = {};\n",
                        ctx.indent(),
                        temp,
                        ctx.type_name(&ty),
                        code
                    )),
                }
            }
            output.push_str(&format!("{}return;\n", ctx.indent()));
        }
        ShaderStage::Fragment | ShaderStage::Surface => {
            let Some(expr) = expr else {
                return Err(ctx.codegen_error("empty return from fragment shader", span));
            };
            let (code, _) = emit_expr(ctx, expr)?;
            match ctx.backend {
                TextShaderBackend::Hlsl | TextShaderBackend::Usf => {
                    let output_struct = format!("{}Output", ctx.shader_name);
                    output.push_str(&format!("{}{} _result;\n", ctx.indent(), output_struct));
                    output.push_str(&format!("{}_result.color = {};\n", ctx.indent(), code));
                    output.push_str(&format!("{}return _result;\n", ctx.indent()));
                }
                TextShaderBackend::Wgsl => {
                    output.push_str(&format!("{}return {};\n", ctx.indent(), code));
                }
            }
        }
        ShaderStage::Vertex => {
            let Some(expr) = expr else {
                return Err(ctx.codegen_error("empty return from vertex shader", span));
            };
            let (code, _) = emit_expr(ctx, expr)?;
            match ctx.backend {
                TextShaderBackend::Hlsl | TextShaderBackend::Usf => {
                    let output_struct = format!("{}Output", ctx.shader_name);
                    output.push_str(&format!("{}{} _result;\n", ctx.indent(), output_struct));
                    output.push_str(&format!("{}_result.position = {};\n", ctx.indent(), code));
                    output.push_str(&format!("{}return _result;\n", ctx.indent()));
                }
                TextShaderBackend::Wgsl => {
                    let output_struct = format!("{}Output", ctx.shader_name);
                    output.push_str(&format!("{}var _result: {output_struct};\n", ctx.indent()));
                    output.push_str(&format!("{}_result.position = {};\n", ctx.indent(), code));
                    output.push_str(&format!("{}return _result;\n", ctx.indent()));
                }
            }
        }
        _ => {}
    }
    Ok(output)
}

fn emit_if_statement(
    ctx: &mut TextContext,
    condition: &Expr,
    then_branch: &Block,
    else_branch: Option<&ElseBranch>,
) -> KainResult<String> {
    let (condition, _) = emit_expr(ctx, condition)?;
    let mut output = String::new();
    output.push_str(&format!("{}if ({condition}) {{\n", ctx.indent()));
    ctx.push_indent();
    output.push_str(&emit_block(ctx, then_branch)?);
    ctx.pop_indent();
    output.push_str(&format!("{}}}", ctx.indent()));
    if let Some(else_branch) = else_branch {
        output.push_str(&emit_else_branch(ctx, else_branch)?);
    } else {
        output.push('\n');
    }
    Ok(output)
}

fn emit_else_branch(ctx: &mut TextContext, else_branch: &ElseBranch) -> KainResult<String> {
    match else_branch {
        ElseBranch::Else(block) => {
            let mut output = String::new();
            output.push_str(" else {\n");
            ctx.push_indent();
            output.push_str(&emit_block(ctx, block)?);
            ctx.pop_indent();
            output.push_str(&format!("{}}}\n", ctx.indent()));
            Ok(output)
        }
        ElseBranch::ElseIf(condition, block, next) => {
            let (condition, _) = emit_expr(ctx, condition)?;
            let mut output = String::new();
            output.push_str(&format!(" else if ({condition}) {{\n"));
            ctx.push_indent();
            output.push_str(&emit_block(ctx, block)?);
            ctx.pop_indent();
            output.push_str(&format!("{}}}", ctx.indent()));
            if let Some(next) = next.as_deref() {
                output.push_str(&emit_else_branch(ctx, next)?);
            } else {
                output.push('\n');
            }
            Ok(output)
        }
    }
}

fn emit_expr(ctx: &mut TextContext, expr: &Expr) -> KainResult<(String, ShaderValueType)> {
    match expr {
        Expr::Ident(name, _) => {
            if let Some(var) = ctx.vars.get(name) {
                Ok((var.name.clone(), var.ty.clone()))
            } else {
                Ok((
                    sanitize_identifier(name, ctx.backend),
                    ShaderValueType::Unknown(name.clone()),
                ))
            }
        }
        Expr::Int(value, _) => Ok((
            format_int_literal(ctx.backend, *value),
            ShaderValueType::Scalar(ScalarKind::I32),
        )),
        Expr::Float(value, _) => Ok((
            format_float_literal(*value),
            ShaderValueType::Scalar(ScalarKind::F32),
        )),
        Expr::Bool(value, _) => Ok((value.to_string(), ShaderValueType::Scalar(ScalarKind::Bool))),
        Expr::String(_, span) | Expr::FString(_, span) => {
            Err(ctx.codegen_error("string expressions in shaders", *span))
        }
        Expr::None(span) => Err(ctx.codegen_error("none literal in shaders", *span)),
        Expr::Binary {
            left, op, right, ..
        } => emit_binary(ctx, left, *op, right),
        Expr::Unary { op, operand, .. } => {
            let (operand, ty) = emit_expr(ctx, operand)?;
            let op = match op {
                UnaryOp::Neg => "-",
                UnaryOp::Not => "!",
                UnaryOp::BitNot => "~",
                UnaryOp::Ref | UnaryOp::RefMut | UnaryOp::Deref => "",
            };
            if op.is_empty() {
                Ok((operand, ty))
            } else {
                Ok((format!("({op}{operand})"), ty))
            }
        }
        Expr::Call { callee, args, span } => {
            let Expr::Ident(name, _) = callee.as_ref() else {
                return Err(ctx.codegen_error("complex callees in shaders", *span));
            };
            emit_function_call(ctx, name, args, *span)
        }
        Expr::MethodCall {
            receiver,
            method,
            args,
            span,
        } => emit_method_call(ctx, receiver, method, args, *span),
        Expr::Field { object, field, .. } => {
            let (object, ty) = emit_expr(ctx, object)?;
            Ok((format!("{object}.{field}"), ty.swizzle(field)))
        }
        Expr::Index { object, index, .. } => {
            let (object, ty) = emit_expr(ctx, object)?;
            let (index, _) = emit_expr(ctx, index)?;
            Ok((format!("{object}[{index}]"), ty.element()))
        }
        Expr::Assign { target, value, .. } => {
            let (target, ty) = emit_expr(ctx, target)?;
            let (value, _) = emit_expr(ctx, value)?;
            Ok((format!("{target} = {value}"), ty))
        }
        Expr::If {
            condition,
            then_branch,
            else_branch,
            span,
        } => emit_if_expression(ctx, condition, then_branch, else_branch.as_deref(), *span),
        Expr::Cast { value, target, .. } => {
            let (value, _) = emit_expr(ctx, value)?;
            let ty = ShaderValueType::from_ast_type(target);
            Ok((format!("{}({value})", ctx.type_name(&ty)), ty))
        }
        Expr::Paren(inner, _) => {
            let (code, ty) = emit_expr(ctx, inner)?;
            Ok((format!("({code})"), ty))
        }
        Expr::Array(items, span) => emit_array_literal(ctx, items, *span),
        Expr::Tuple(items, span) => emit_array_literal(ctx, items, *span),
        Expr::Block(block, _) => {
            if block.stmts.len() == 1 {
                if let Stmt::Expr(expr) = &block.stmts[0] {
                    return emit_expr(ctx, expr);
                }
            }
            Err(ctx.codegen_error("block expressions in shaders", expr.span()))
        }
        _ => Err(ctx.codegen_error(format!("expression shape {expr:?}"), expr.span())),
    }
}

fn emit_binary(
    ctx: &mut TextContext,
    left: &Expr,
    op: BinaryOp,
    right: &Expr,
) -> KainResult<(String, ShaderValueType)> {
    let (left_code, left_ty) = emit_expr(ctx, left)?;
    let (right_code, _) = emit_expr(ctx, right)?;
    let op_str = match op {
        BinaryOp::Add => "+",
        BinaryOp::Sub => "-",
        BinaryOp::Mul => "*",
        BinaryOp::Div => "/",
        BinaryOp::Mod => "%",
        BinaryOp::Eq => "==",
        BinaryOp::Ne => "!=",
        BinaryOp::Lt => "<",
        BinaryOp::Le => "<=",
        BinaryOp::Gt => ">",
        BinaryOp::Ge => ">=",
        BinaryOp::And => "&&",
        BinaryOp::Or => "||",
        BinaryOp::BitAnd => "&",
        BinaryOp::BitOr => "|",
        BinaryOp::BitXor => "^",
        BinaryOp::Shl => "<<",
        BinaryOp::Shr => ">>",
        BinaryOp::Assign => "=",
        BinaryOp::AddAssign => "+=",
        BinaryOp::SubAssign => "-=",
        BinaryOp::MulAssign => "*=",
        BinaryOp::DivAssign => "/=",
        BinaryOp::Pow | BinaryOp::Range | BinaryOp::RangeInclusive => {
            return Err(ctx.codegen_error(format!("binary operator {op:?}"), left.span()))
        }
    };
    let ty = match op {
        BinaryOp::Eq
        | BinaryOp::Ne
        | BinaryOp::Lt
        | BinaryOp::Le
        | BinaryOp::Gt
        | BinaryOp::Ge
        | BinaryOp::And
        | BinaryOp::Or => ShaderValueType::Scalar(ScalarKind::Bool),
        _ => left_ty,
    };
    Ok((format!("({left_code} {op_str} {right_code})"), ty))
}

fn emit_if_expression(
    ctx: &mut TextContext,
    condition: &Expr,
    then_branch: &Block,
    else_branch: Option<&ElseBranch>,
    span: Span,
) -> KainResult<(String, ShaderValueType)> {
    if ctx.backend != TextShaderBackend::Hlsl && ctx.backend != TextShaderBackend::Usf {
        return Err(ctx.codegen_error(
            "if expressions; use statement-style if in WGSL shaders",
            span,
        ));
    }
    if then_branch.stmts.len() != 1 {
        return Err(ctx.codegen_error("multi-statement if expressions", span));
    }
    let Some(ElseBranch::Else(else_block)) = else_branch else {
        return Err(ctx.codegen_error("if expressions without else", span));
    };
    if else_block.stmts.len() != 1 {
        return Err(ctx.codegen_error("multi-statement else expressions", span));
    }
    let Stmt::Expr(then_expr) = &then_branch.stmts[0] else {
        return Err(ctx.codegen_error("non-expression then branch", span));
    };
    let Stmt::Expr(else_expr) = &else_block.stmts[0] else {
        return Err(ctx.codegen_error("non-expression else branch", span));
    };
    let (condition, _) = emit_expr(ctx, condition)?;
    let (then_code, then_ty) = emit_expr(ctx, then_expr)?;
    let (else_code, _) = emit_expr(ctx, else_expr)?;
    Ok((
        format!("({condition} ? {then_code} : {else_code})"),
        then_ty,
    ))
}

fn emit_array_literal(
    ctx: &mut TextContext,
    items: &[Expr],
    span: Span,
) -> KainResult<(String, ShaderValueType)> {
    if items.is_empty() {
        return Err(ctx.codegen_error("empty array or tuple literals in shaders", span));
    }
    let mut codes = Vec::with_capacity(items.len());
    let mut item_ty = None;
    for item in items {
        let (code, ty) = emit_expr(ctx, item)?;
        codes.push(code);
        item_ty.get_or_insert(ty);
    }
    let item_ty = item_ty.unwrap_or(ShaderValueType::Scalar(ScalarKind::F32));
    match ctx.backend {
        TextShaderBackend::Hlsl | TextShaderBackend::Usf => Ok((
            format!("{{{}}}", codes.join(", ")),
            ShaderValueType::Unknown("array".to_string()),
        )),
        TextShaderBackend::Wgsl => Ok((
            format!(
                "array<{}, {}>({})",
                ctx.type_name(&item_ty),
                items.len(),
                codes.join(", ")
            ),
            ShaderValueType::Unknown("array".to_string()),
        )),
    }
}

fn emit_function_call(
    ctx: &mut TextContext,
    name: &str,
    args: &[CallArg],
    span: Span,
) -> KainResult<(String, ShaderValueType)> {
    reject_backend_intrinsic(ctx, name, span)?;
    if let Some((ctor, ty)) = constructor_name(ctx.backend, name) {
        let args = emit_call_args(ctx, args)?;
        return Ok((format!("{ctor}({})", args.join(", ")), ty));
    }

    let mapped = match ctx.backend {
        TextShaderBackend::Hlsl | TextShaderBackend::Usf => hlsl_function_name(name),
        TextShaderBackend::Wgsl => wgsl_function_name(name),
    };
    match mapped {
        FunctionMapping::Direct(target) => {
            let arg_values = emit_call_args(ctx, args)?;
            let ty = infer_call_return_type(ctx, name, args, span)?;
            Ok((format!("{target}({})", arg_values.join(", ")), ty))
        }
        FunctionMapping::SameName => {
            let arg_values = emit_call_args(ctx, args)?;
            let ty = infer_call_return_type(ctx, name, args, span)?;
            Ok((format!("{name}({})", arg_values.join(", ")), ty))
        }
        FunctionMapping::Sample => emit_texture_sample(ctx, args, span),
        FunctionMapping::Unsupported(reason) => Err(ctx.codegen_error(reason, span)),
    }
}

fn emit_method_call(
    ctx: &mut TextContext,
    receiver: &Expr,
    method: &str,
    args: &[CallArg],
    span: Span,
) -> KainResult<(String, ShaderValueType)> {
    let (receiver, receiver_ty) = emit_expr(ctx, receiver)?;
    let mut arg_values = emit_call_args(ctx, args)?;
    match (ctx.backend, method) {
        (TextShaderBackend::Hlsl | TextShaderBackend::Usf, "Sample") => {
            let Some(coords) = arg_values.first() else {
                return Err(ctx.codegen_error("Texture.Sample without coordinates", span));
            };
            Ok((
                format!("{receiver}.Sample({receiver}_sampler, {coords})"),
                ShaderValueType::Vector(ScalarKind::F32, 4),
            ))
        }
        (TextShaderBackend::Wgsl, "Sample") => {
            let Some(coords) = arg_values.first() else {
                return Err(ctx.codegen_error("texture Sample without coordinates", span));
            };
            Ok((
                format!("textureSample({receiver}, {receiver}_sampler, {coords})"),
                ShaderValueType::Vector(ScalarKind::F32, 4),
            ))
        }
        (_, "Load") => {
            let Some(location) = arg_values.first_mut() else {
                return Err(ctx.codegen_error("texture Load without coordinates", span));
            };
            let code = match ctx.backend {
                TextShaderBackend::Hlsl | TextShaderBackend::Usf => {
                    format!("{receiver}.Load({location})")
                }
                TextShaderBackend::Wgsl => format!("textureLoad({receiver}, {location}, 0)"),
            };
            Ok((code, ShaderValueType::Vector(ScalarKind::F32, 4)))
        }
        _ => Err(ctx.codegen_error(
            format!("method call '.{method}' on {}", ctx.type_name(&receiver_ty)),
            span,
        )),
    }
}

fn emit_call_args(ctx: &mut TextContext, args: &[CallArg]) -> KainResult<Vec<String>> {
    let mut values = Vec::with_capacity(args.len());
    for arg in args {
        values.push(emit_expr(ctx, &arg.value)?.0);
    }
    Ok(values)
}

enum FunctionMapping {
    Direct(&'static str),
    SameName,
    Sample,
    Unsupported(&'static str),
}

fn hlsl_function_name(name: &str) -> FunctionMapping {
    match name {
        "fract" => FunctionMapping::Direct("frac"),
        "mix" => FunctionMapping::Direct("lerp"),
        "to_float" => FunctionMapping::Direct("float"),
        "to_int" => FunctionMapping::Direct("int"),
        "to_uint" => FunctionMapping::Direct("uint"),
        "sample" | "sample_lod" | "sample_grad" | "sample_bias" | "sample_cmp" => {
            FunctionMapping::Sample
        }
        // Wave 2 DELTA: subgroup intrinsics → HLSL WaveActive mappings
        "cuda_lane_id" => FunctionMapping::Direct("WaveGetLaneIndex"),
        "cuda_active_mask" => FunctionMapping::Direct("WaveActiveBallot"),
        "cuda_ballot" => FunctionMapping::Direct("WaveActiveBallot"),
        "cuda_warp_any" => FunctionMapping::Direct("WaveActiveAnyTrue"),
        "cuda_warp_all" => FunctionMapping::Direct("WaveActiveAllTrue"),
        "cuda_warp_reduce_sum_f32" | "cuda_warp_reduce_sum_u32" => {
            FunctionMapping::Direct("WaveActiveSum")
        }
        "cuda_warp_reduce_max_f32" | "cuda_warp_reduce_max_u32" => {
            FunctionMapping::Direct("WaveActiveMax")
        }
        "cuda_warp_reduce_min_f32" | "cuda_warp_reduce_min_u32" => {
            FunctionMapping::Direct("WaveActiveMin")
        }
        "cuda_shfl_xor_u32" | "cuda_shfl_xor_f32" => {
            FunctionMapping::Direct("WaveReadLaneAt")
        }
        "cuda_block_sync" | "cuda_warp_sync" | "cuda_barrier_sync" => {
            FunctionMapping::Direct("GroupMemoryBarrierWithGroupSync")
        }
        "sin" | "cos" | "tan" | "asin" | "acos" | "atan" | "atan2" | "abs" | "floor" | "ceil"
        | "round" | "trunc" | "sqrt" | "rsqrt" | "exp" | "exp2" | "log" | "log2" | "log10"
        | "sign" | "saturate" | "pow" | "min" | "max" | "fmod" | "step" | "clamp"
        | "smoothstep" | "mad" | "length" | "distance" | "normalize" | "dot" | "cross"
        | "reflect" | "refract" | "faceforward" | "transpose" | "determinant" | "ddx" | "ddy"
        | "ddx_fine" | "ddy_fine" | "ddx_coarse" | "ddy_coarse" | "fwidth" | "all" | "any"
        | "countbits" | "firstbithigh" | "firstbitlow" | "reversebits" => FunctionMapping::SameName,
        "texture_size" | "texture_query_lod" | "texture_gather" => {
            FunctionMapping::Unsupported("advanced texture query calls in direct HLSL")
        }
        _ => FunctionMapping::Unsupported("unknown function call"),
    }
}

fn wgsl_function_name(name: &str) -> FunctionMapping {
    match name {
        "fract" => FunctionMapping::Direct("fract"),
        "mix" => FunctionMapping::Direct("mix"),
        "lerp" => FunctionMapping::Direct("mix"),
        "to_float" | "Float" => FunctionMapping::Direct("f32"),
        "to_int" | "Int" => FunctionMapping::Direct("i32"),
        "to_uint" | "UInt" => FunctionMapping::Direct("u32"),
        "sample" => FunctionMapping::Sample,
        "faceforward" => FunctionMapping::Direct("faceForward"),
        "rsqrt" => FunctionMapping::Direct("inverseSqrt"),
        "ddx" | "ddx_fine" | "ddx_coarse" => FunctionMapping::Direct("dpdx"),
        "ddy" | "ddy_fine" | "ddy_coarse" => FunctionMapping::Direct("dpdy"),
        "countbits" => FunctionMapping::Direct("countOneBits"),
        "firstbithigh" => FunctionMapping::Direct("firstLeadingBit"),
        "firstbitlow" => FunctionMapping::Direct("firstTrailingBit"),
        "reversebits" => FunctionMapping::Direct("reverseBits"),
        // Wave 2 DELTA: subgroup intrinsics → WGSL builtins
        "cuda_lane_id" => FunctionMapping::Direct("subgroup_invocation_id"),
        "cuda_warp_id" => FunctionMapping::Direct("subgroup_id"),
        "cuda_warp_reduce_sum_f32" | "cuda_warp_reduce_sum_u32" => {
            FunctionMapping::Direct("subgroupAdd")
        }
        "cuda_warp_reduce_max_f32" | "cuda_warp_reduce_max_u32" => {
            FunctionMapping::Direct("subgroupMax")
        }
        "cuda_warp_reduce_min_f32" | "cuda_warp_reduce_min_u32" => {
            FunctionMapping::Direct("subgroupMin")
        }
        "cuda_ballot" => FunctionMapping::Direct("subgroupBallot"),
        "cuda_warp_any" => FunctionMapping::Direct("subgroupAny"),
        "cuda_warp_all" => FunctionMapping::Direct("subgroupAll"),
        "cuda_shfl_xor_u32" | "cuda_shfl_xor_f32" => {
            FunctionMapping::Direct("subgroupShuffleXor")
        }
        "cuda_block_sync" | "cuda_warp_sync" => FunctionMapping::Direct("subgroupBarrier"),
        "cuda_barrier_sync" => FunctionMapping::Direct("workgroupBarrier"),
        "sin" | "cos" | "tan" | "asin" | "acos" | "atan" | "atan2" | "abs" | "floor" | "ceil"
        | "round" | "trunc" | "sqrt" | "exp" | "exp2" | "log" | "log2" | "sign" | "pow" | "min"
        | "max" | "clamp" | "smoothstep" | "length" | "distance" | "normalize" | "dot"
        | "cross" | "reflect" | "refract" | "faceForward" | "transpose" | "determinant"
        | "dpdx" | "dpdy" | "fwidth" | "all" | "any" | "countOneBits" | "firstLeadingBit"
        | "firstTrailingBit" | "reverseBits" | "inverseSqrt" => FunctionMapping::SameName,
        "saturate" => FunctionMapping::Unsupported("saturate; use clamp(x, 0.0, 1.0)"),
        "fmod" | "mad" => FunctionMapping::Unsupported("HLSL-only function spelling in WGSL"),
        "sample_lod" | "sample_grad" | "sample_bias" | "sample_cmp" | "texture_size"
        | "texture_query_lod" | "texture_gather" => {
            FunctionMapping::Unsupported("advanced texture calls in direct WGSL")
        }
        _ => FunctionMapping::Unsupported("unknown function call"),
    }
}

fn constructor_name(backend: TextShaderBackend, name: &str) -> Option<(String, ShaderValueType)> {
    let ty = match name {
        "float" | "Float" | "f32" => ShaderValueType::Scalar(ScalarKind::F32),
        "int" | "Int" | "i32" => ShaderValueType::Scalar(ScalarKind::I32),
        "uint" | "UInt" | "u32" => ShaderValueType::Scalar(ScalarKind::U32),
        "bool" | "Bool" => ShaderValueType::Scalar(ScalarKind::Bool),
        "vec2" | "Vec2" => ShaderValueType::Vector(ScalarKind::F32, 2),
        "vec3" | "Vec3" => ShaderValueType::Vector(ScalarKind::F32, 3),
        "vec4" | "Vec4" => ShaderValueType::Vector(ScalarKind::F32, 4),
        "ivec2" | "IVec2" => ShaderValueType::Vector(ScalarKind::I32, 2),
        "ivec3" | "IVec3" => ShaderValueType::Vector(ScalarKind::I32, 3),
        "ivec4" | "IVec4" => ShaderValueType::Vector(ScalarKind::I32, 4),
        "uvec2" | "UVec2" => ShaderValueType::Vector(ScalarKind::U32, 2),
        "uvec3" | "UVec3" => ShaderValueType::Vector(ScalarKind::U32, 3),
        "uvec4" | "UVec4" => ShaderValueType::Vector(ScalarKind::U32, 4),
        "mat2" | "Mat2" => ShaderValueType::Matrix(2, 2),
        "mat3" | "Mat3" => ShaderValueType::Matrix(3, 3),
        "mat4" | "Mat4" => ShaderValueType::Matrix(4, 4),
        _ => return None,
    };
    let ctor = match backend {
        TextShaderBackend::Hlsl | TextShaderBackend::Usf => ty.hlsl_name(),
        TextShaderBackend::Wgsl => ty.wgsl_name(),
    };
    Some((ctor, ty))
}

fn emit_texture_sample(
    ctx: &mut TextContext,
    args: &[CallArg],
    span: Span,
) -> KainResult<(String, ShaderValueType)> {
    if args.len() < 2 {
        return Err(ctx.codegen_error("texture sample with fewer than two arguments", span));
    }
    let (texture, _) = emit_expr(ctx, &args[0].value)?;
    let (coords, _) = emit_expr(ctx, &args[1].value)?;
    let code = match ctx.backend {
        TextShaderBackend::Hlsl | TextShaderBackend::Usf => {
            format!("{texture}.Sample({texture}_sampler, {coords})")
        }
        TextShaderBackend::Wgsl => {
            format!("textureSample({texture}, {texture}_sampler, {coords})")
        }
    };
    Ok((code, ShaderValueType::Vector(ScalarKind::F32, 4)))
}

fn infer_call_return_type(
    ctx: &mut TextContext,
    name: &str,
    args: &[CallArg],
    span: Span,
) -> KainResult<ShaderValueType> {
    let first_ty = if let Some(first) = args.first() {
        emit_expr(ctx, &first.value)?.1
    } else {
        ShaderValueType::Scalar(ScalarKind::F32)
    };
    Ok(match name {
        "all" | "any" => ShaderValueType::Scalar(ScalarKind::Bool),
        "length" | "distance" | "dot" | "determinant" => ShaderValueType::Scalar(ScalarKind::F32),
        "to_float" => ShaderValueType::Scalar(ScalarKind::F32),
        "to_int" => ShaderValueType::Scalar(ScalarKind::I32),
        "to_uint" => ShaderValueType::Scalar(ScalarKind::U32),
        "sample" => ShaderValueType::Vector(ScalarKind::F32, 4),
        _ if first_ty.is_boolish() => first_ty,
        _ => {
            if args.is_empty() {
                return Err(ctx.codegen_error(
                    format!("function '{name}' with no type-inferable arguments"),
                    span,
                ));
            }
            first_ty
        }
    })
}

fn reject_backend_intrinsic(ctx: &TextContext, name: &str, span: Span) -> KainResult<()> {
    if name.starts_with("cuda_") {
        // Wave 2 DELTA: allow cuda_* subgroup intrinsics when they have
        // a backend-specific mapping (HLSL → WaveActive*, WGSL → subgroup*).
        if is_subgroup_intrinsic(name) {
            return Ok(());
        }
        return Err(ctx.codegen_error(
            format!(
                "CUDA/PTX-only intrinsic '{name}' in {} output",
                ctx.backend.label()
            ),
            span,
        ));
    }
    if ctx.backend == TextShaderBackend::Wgsl && name.starts_with("wave_") {
        return Err(ctx.codegen_error(format!("HLSL wave intrinsic '{name}' in WGSL output"), span));
    }
    Ok(())
}

/// Returns true if `name` is a cuda_* subgroup intrinsic that has
/// backend-appropriate mappings in both HLSL and WGSL.
fn is_subgroup_intrinsic(name: &str) -> bool {
    matches!(
        name,
        "cuda_lane_id"
            | "cuda_warp_id"
            | "cuda_active_mask"
            | "cuda_ballot"
            | "cuda_warp_any"
            | "cuda_warp_all"
            | "cuda_shfl_xor_u32"
            | "cuda_shfl_xor_f32"
            | "cuda_warp_reduce_sum_f32"
            | "cuda_warp_reduce_sum_u32"
            | "cuda_warp_reduce_max_f32"
            | "cuda_warp_reduce_max_u32"
            | "cuda_warp_reduce_min_f32"
            | "cuda_warp_reduce_min_u32"
            | "cuda_block_sync"
            | "cuda_warp_sync"
            | "cuda_barrier_sync"
    )
}

fn format_float_literal(value: f64) -> String {
    if value.is_finite() {
        let mut text = format!("{value:.6}");
        while text.contains('.') && text.ends_with('0') {
            text.pop();
        }
        if text.ends_with('.') {
            text.push('0');
        }
        text
    } else {
        value.to_string()
    }
}

fn format_int_literal(backend: TextShaderBackend, value: i64) -> String {
    match backend {
        TextShaderBackend::Wgsl if value >= 0 => format!("{value}"),
        _ => value.to_string(),
    }
}

fn block_has_return(block: &Block) -> bool {
    block
        .stmts
        .iter()
        .any(|stmt| matches!(stmt, Stmt::Return(_, _)))
}
