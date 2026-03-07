use crate::codegen_rust::gpu_artifacts::{
    collect_gpu_artifacts, RustGpuArtifactOutput, RustGpuBindingKind, RustGpuShaderStage,
};
use kain_core::types::TypedProgram;

pub fn generate_gpu_host(program: &TypedProgram) -> String {
    let artifacts = collect_gpu_artifacts(program);
    render_gpu_host(&artifacts)
}

pub fn render_gpu_host(artifacts: &RustGpuArtifactOutput) -> String {
    let mut out = String::new();
    out.push_str("#![allow(dead_code)]\n");
    out.push_str("#![allow(unused_variables)]\n\n");
    out.push_str("pub mod kain_gpu_generated {\n");
    out.push_str("    #[derive(Debug, Clone, Copy, PartialEq, Eq)]\n");
    out.push_str("    pub enum ShaderStage {\n");
    out.push_str("        Vertex,\n");
    out.push_str("        Fragment,\n");
    out.push_str("        Compute,\n");
    out.push_str("        Surface,\n");
    out.push_str("    }\n\n");
    out.push_str("    #[derive(Debug, Clone, Copy, PartialEq, Eq)]\n");
    out.push_str("    pub enum BindingKind {\n");
    out.push_str("        StorageBuffer,\n");
    out.push_str("        Sampler2D,\n");
    out.push_str("        Uniform,\n");
    out.push_str("        LocalSize,\n");
    out.push_str("        SpecializationConstant,\n");
    out.push_str("    }\n\n");
    out.push_str("    #[derive(Debug, Clone, Copy)]\n");
    out.push_str("    pub struct BindingDesc {\n");
    out.push_str("        pub name: &'static str,\n");
    out.push_str("        pub binding: u32,\n");
    out.push_str("        pub descriptor_set: u32,\n");
    out.push_str("        pub ty: &'static str,\n");
    out.push_str("        pub kind: BindingKind,\n");
    out.push_str("    }\n\n");
    out.push_str("    #[derive(Debug, Clone, Copy)]\n");
    out.push_str("    pub struct BindingLayoutEntry {\n");
    out.push_str("        pub binding: u32,\n");
    out.push_str("        pub descriptor_set: u32,\n");
    out.push_str("        pub kind: BindingKind,\n");
    out.push_str("        pub ty: &'static str,\n");
    out.push_str("    }\n\n");
    out.push_str("    #[derive(Debug, Clone, Copy)]\n");
    out.push_str("    pub struct DispatchSize {\n");
    out.push_str("        pub x: u32,\n");
    out.push_str("        pub y: u32,\n");
    out.push_str("        pub z: u32,\n");
    out.push_str("    }\n\n");
    out.push_str("    #[derive(Debug, Clone)]\n");
    out.push_str("    pub struct BuiltinInputParam {\n");
    out.push_str("        pub name: &'static str,\n");
    out.push_str("        pub ty: &'static str,\n");
    out.push_str("    }\n\n");
    out.push_str("    #[derive(Debug, Clone)]\n");
    out.push_str("    pub struct UniformParam {\n");
    out.push_str("        pub ty: &'static str,\n");
    out.push_str("    }\n\n");
    out.push_str("    #[derive(Debug, Clone)]\n");
    out.push_str("    pub struct StorageBufferParam {\n");
    out.push_str("        pub ty: &'static str,\n");
    out.push_str("        pub read_only: bool,\n");
    out.push_str("    }\n\n");
    out.push_str("    #[derive(Debug, Clone)]\n");
    out.push_str("    pub struct Sampler2DParam {\n");
    out.push_str("        pub ty: &'static str,\n");
    out.push_str("    }\n\n");
    out.push_str("    #[derive(Debug, Clone)]\n");
    out.push_str("    pub struct LocalSizeParam {\n");
    out.push_str("        pub axis: &'static str,\n");
    out.push_str("        pub default_value: u32,\n");
    out.push_str("    }\n\n");
    out.push_str("    #[derive(Debug, Clone)]\n");
    out.push_str("    pub struct SpecializationConstantParam {\n");
    out.push_str("        pub ty: &'static str,\n");
    out.push_str("    }\n\n");
    out.push_str("    #[derive(Debug, Clone)]\n");
    out.push_str("    pub struct DispatchCall<'a, TParams> {\n");
    out.push_str("        pub entry_point: &'static str,\n");
    out.push_str("        pub stage: ShaderStage,\n");
    out.push_str("        pub size: DispatchSize,\n");
    out.push_str("        pub params: &'a TParams,\n");
    out.push_str("    }\n\n");
    out.push_str("    #[derive(Debug, Clone, Copy)]\n");
    out.push_str("    pub struct ShaderDesc {\n");
    out.push_str("        pub name: &'static str,\n");
    out.push_str("        pub stage: ShaderStage,\n");
    out.push_str("        pub entry_point: &'static str,\n");
    out.push_str("        pub output_type: &'static str,\n");
    out.push_str("        pub bindings: &'static [BindingDesc],\n");
    out.push_str("    }\n\n");

    for shader in &artifacts.shaders {
        let shader_mod = sanitize_ident(&shader.name);
        out.push_str(&format!("    pub mod {} {{\n", shader_mod));
        out.push_str("        use super::{\n");
        out.push_str("            BindingDesc, BindingKind, BindingLayoutEntry, BuiltinInputParam, DispatchCall,\n");
        out.push_str("            DispatchSize, LocalSizeParam, Sampler2DParam, ShaderDesc, ShaderStage,\n");
        out.push_str("            SpecializationConstantParam, StorageBufferParam, UniformParam,\n");
        out.push_str("        };\n\n");

        out.push_str("        #[derive(Debug, Clone)]\n");
        out.push_str("        pub struct Params {\n");
        for input in &shader.inputs {
            out.push_str(&format!(
                "            pub {}: BuiltinInputParam,\n",
                sanitize_ident(&input.name)
            ));
        }
        for binding in &shader.bindings {
            out.push_str(&format!(
                "            pub {}: {},\n",
                sanitize_ident(&binding.name),
                binding_param_type(binding.kind)
            ));
        }
        out.push_str("        }\n\n");

        out.push_str("        impl Default for Params {\n");
        out.push_str("            fn default() -> Self {\n");
        out.push_str("                Self {\n");
        for input in &shader.inputs {
            out.push_str(&format!(
                "                    {}: BuiltinInputParam {{ name: \"{}\", ty: \"{}\" }},\n",
                sanitize_ident(&input.name),
                input.name,
                input.ty
            ));
        }
        for binding in &shader.bindings {
            out.push_str(&format!(
                "                    {}: {},\n",
                sanitize_ident(&binding.name),
                default_binding_initializer(binding)
            ));
        }
        out.push_str("                }\n");
        out.push_str("            }\n");
        out.push_str("        }\n\n");

        out.push_str("        pub const BINDINGS: &[BindingDesc] = &[\n");
        for binding in &shader.bindings {
            out.push_str("            BindingDesc { ");
            out.push_str(&format!("name: \"{}\", ", binding.name));
            out.push_str(&format!("binding: {}, ", binding.binding));
            out.push_str(&format!("descriptor_set: {}, ", binding.descriptor_set));
            out.push_str(&format!("ty: \"{}\", ", binding.ty));
            out.push_str(&format!("kind: BindingKind::{}, ", binding_kind_name(binding.kind)));
            out.push_str("},\n");
        }
        out.push_str("        ];\n\n");

        out.push_str("        pub const SHADER: ShaderDesc = ShaderDesc { ");
        out.push_str(&format!("name: \"{}\", ", shader.name));
        out.push_str(&format!("stage: ShaderStage::{}, ", stage_name(shader.stage)));
        out.push_str(&format!("entry_point: \"{}\", ", shader.entry_point));
        out.push_str(&format!("output_type: \"{}\", ", shader.output_type));
        out.push_str("bindings: BINDINGS, ");
        out.push_str("};\n\n");

        out.push_str("        pub fn descriptor() -> &'static ShaderDesc {\n");
        out.push_str("            &SHADER\n");
        out.push_str("        }\n\n");

        out.push_str("        pub fn descriptor_layout() -> Vec<BindingLayoutEntry> {\n");
        out.push_str("            BINDINGS\n");
        out.push_str("                .iter()\n");
        out.push_str("                .map(|binding| BindingLayoutEntry {\n");
        out.push_str("                    binding: binding.binding,\n");
        out.push_str("                    descriptor_set: binding.descriptor_set,\n");
        out.push_str("                    kind: binding.kind,\n");
        out.push_str("                    ty: binding.ty,\n");
        out.push_str("                })\n");
        out.push_str("                .collect()\n");
        out.push_str("        }\n\n");

        out.push_str("        pub fn dispatch<'a>(params: &'a Params, x: u32, y: u32, z: u32) -> DispatchCall<'a, Params> {\n");
        out.push_str("            DispatchCall {\n");
        out.push_str(&format!("                entry_point: \"{}\",\n", shader.entry_point));
        out.push_str(&format!("                stage: ShaderStage::{},\n", stage_name(shader.stage)));
        out.push_str("                size: DispatchSize { x, y, z },\n");
        out.push_str("                params,\n");
        out.push_str("            }\n");
        out.push_str("        }\n");
        out.push_str("    }\n\n");
    }

    out.push_str("    pub fn shaders() -> &'static [ShaderDesc] {\n");
    out.push_str("        &[\n");
    for shader in &artifacts.shaders {
        out.push_str(&format!("            {}::SHADER,\n", sanitize_ident(&shader.name)));
    }
    out.push_str("        ]\n");
    out.push_str("    }\n");
    out.push_str("}\n");
    out
}

fn stage_name(stage: RustGpuShaderStage) -> &'static str {
    match stage {
        RustGpuShaderStage::Vertex => "Vertex",
        RustGpuShaderStage::Fragment => "Fragment",
        RustGpuShaderStage::Compute => "Compute",
        RustGpuShaderStage::Surface => "Surface",
    }
}

fn binding_kind_name(kind: RustGpuBindingKind) -> &'static str {
    match kind {
        RustGpuBindingKind::StorageBuffer => "StorageBuffer",
        RustGpuBindingKind::Sampler2D => "Sampler2D",
        RustGpuBindingKind::Uniform => "Uniform",
        RustGpuBindingKind::LocalSize => "LocalSize",
        RustGpuBindingKind::SpecializationConstant => "SpecializationConstant",
    }
}

fn binding_param_type(kind: RustGpuBindingKind) -> &'static str {
    match kind {
        RustGpuBindingKind::StorageBuffer => "StorageBufferParam",
        RustGpuBindingKind::Sampler2D => "Sampler2DParam",
        RustGpuBindingKind::Uniform => "UniformParam",
        RustGpuBindingKind::LocalSize => "LocalSizeParam",
        RustGpuBindingKind::SpecializationConstant => "SpecializationConstantParam",
    }
}

fn default_binding_initializer(binding: &crate::codegen_rust::gpu_artifacts::RustGpuBindingArtifact) -> String {
    match binding.kind {
        RustGpuBindingKind::StorageBuffer => {
            format!("StorageBufferParam {{ ty: \"{}\", read_only: false }}", binding.ty)
        }
        RustGpuBindingKind::Sampler2D => {
            format!("Sampler2DParam {{ ty: \"{}\" }}", binding.ty)
        }
        RustGpuBindingKind::Uniform => {
            format!("UniformParam {{ ty: \"{}\" }}", binding.ty)
        }
        RustGpuBindingKind::LocalSize => {
            let axis = binding.name.strip_prefix("LOCAL_SIZE_").unwrap_or(&binding.name);
            let default_value = if axis == "Z" { 1 } else { 8 };
            format!("LocalSizeParam {{ axis: \"{}\", default_value: {} }}", axis, default_value)
        }
        RustGpuBindingKind::SpecializationConstant => {
            format!("SpecializationConstantParam {{ ty: \"{}\" }}", binding.ty)
        }
    }
}

fn sanitize_ident(name: &str) -> String {
    let mut out = String::new();
    for ch in name.chars() {
        if ch.is_ascii_alphanumeric() || ch == '_' {
            out.push(ch.to_ascii_lowercase());
        } else {
            out.push('_');
        }
    }
    if out.is_empty() {
        "binding".to_string()
    } else if out.chars().next().is_some_and(|ch| ch.is_ascii_digit()) {
        format!("binding_{}", out)
    } else {
        out
    }
}
