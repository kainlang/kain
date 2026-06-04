//! Raw PTX generation for KAIN compute shaders.
//!
//! This backend emits PTX text directly from Kain's typed shader AST. It is
//! intentionally compute-only: SPIR-V remains the canonical artifact payload,
//! while PTX is a driver-JIT friendly peer output for NVIDIA devices.

use crate::ptx_module::{
    PtxAddressSpace as ModuleAddressSpace, PtxArch as ModuleArch,
    PtxKernelParam as ModuleKernelParam, PtxKernelParamEncoding as ModuleKernelParamEncoding,
    PtxKernelPlan as ModuleKernelPlan, PtxLaunchConfig as ModuleLaunchConfig,
    PtxScalarKind as ModuleScalarKind, PtxSharedOpKind as ModuleSharedOpKind,
    PtxTensorFixedKind as ModuleTensorFixedKind, PtxTensorOpRequest as ModuleTensorOpRequest,
    PtxWarpOpKind as ModuleWarpOpKind, DEFAULT_PTX_ARCH, DEFAULT_PTX_VERSION, PTX_ARCH_SPECS,
};
use kain_core::ast::{
    BinaryOp, Block, CallArg, ElseBranch, Expr, Pattern, ShaderStage, Stmt, Type, UnaryOp,
};
use kain_core::builder::{CodegenDiagnostic, ShaderDiagnostic};
use kain_core::error::{CompilerPhase, KainError, KainResult};
use kain_core::gpu_storage_element_stride_bytes;
use kain_core::span::Span;
use kain_core::types::{TypedItem, TypedProgram, TypedShader};
use kain_core::DiagnosticCode;
use kain_core::DiagnosticSemanticPacket;
use kain_semantic::enrich_report as enrich_semantic_report;
use std::collections::{HashMap, HashSet};

#[path = "ptx_surface.rs"]
mod ptx_surface;
use ptx_surface::{binary_op_inst, cmp_inst, cvt_inst, SuffixMode};

const PTX_VERSION: &str = DEFAULT_PTX_VERSION;
const DEFAULT_LOCAL_SIZE: [u32; 3] = [8, 8, 1];
const KAIN_PTX_ARCH_ENV: &str = "KAIN_PTX_ARCH";
const KAIN_CUDA_ARCH_ENV: &str = "KAIN_CUDA_ARCH";

pub fn generate(program: &TypedProgram) -> KainResult<String> {
    generate_with_options(program, PtxCodegenOptions::from_env()?)
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct GeneratedPtxModule {
    pub target_arch: ModuleArch,
    pub kernel_minimum_arch: ModuleArch,
    pub ptx: String,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum PtxVariantSelection {
    PrimaryOnly,
    AutoFamily,
    Explicit(Vec<ModuleArch>),
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct PtxCodegenOptions {
    pub target_arch: Option<ModuleArch>,
}

impl PtxCodegenOptions {
    pub const fn auto() -> Self {
        Self { target_arch: None }
    }

    pub const fn with_target_arch(target_arch: ModuleArch) -> Self {
        Self {
            target_arch: Some(target_arch),
        }
    }

    pub fn from_env() -> KainResult<Self> {
        Ok(Self {
            target_arch: parse_target_arch_override_from_env()?,
        })
    }

    pub fn resolve_module_target_arch(self, required_arch: ModuleArch) -> ModuleArch {
        self.target_arch
            .unwrap_or(std::cmp::max(DEFAULT_PTX_ARCH, required_arch))
    }
}

struct PreparedPtxShader {
    name: String,
    span: Span,
    emitted_ptx: String,
    kernel_plan: ModuleKernelPlan,
}

pub fn generate_variant_modules(
    program: &TypedProgram,
    options: PtxCodegenOptions,
    selection: PtxVariantSelection,
) -> KainResult<Vec<GeneratedPtxModule>> {
    let prepared = prepare_program_shaders(program)?;
    let required_arch = prepared
        .iter()
        .map(|shader| shader.kernel_plan.minimum_arch())
        .max()
        .unwrap_or(DEFAULT_PTX_ARCH);
    let target_arches = resolve_variant_target_arches(options, required_arch, selection)?;

    target_arches
        .into_iter()
        .map(|target_arch| {
            Ok(GeneratedPtxModule {
                target_arch,
                kernel_minimum_arch: required_arch,
                ptx: render_ptx_module(&prepared, target_arch)?,
            })
        })
        .collect()
}

pub fn generate_with_options(
    program: &TypedProgram,
    options: PtxCodegenOptions,
) -> KainResult<String> {
    Ok(
        generate_variant_modules(program, options, PtxVariantSelection::PrimaryOnly)?
            .into_iter()
            .next()
            .expect("PTX module generation validated non-empty program")
            .ptx,
    )
}

fn prepare_program_shaders(program: &TypedProgram) -> KainResult<Vec<PreparedPtxShader>> {
    let mut prepared = Vec::new();
    for item in &program.items {
        if let TypedItem::Shader(shader) = item {
            prepared.push(prepare_shader(shader)?);
        }
    }

    if prepared.is_empty() {
        let report = CodegenDiagnostic::backend_failed(
            "PTX backend emitted no kernels: the typed program contains no shader items",
        )
        .help("The PTX backend targets `shader compute` items. Check that your source declares at least one compute shader.");
        return Err(enriched_codegen_error(
            report,
            "",
            DiagnosticCode::CodegenGeneric,
            CompilerPhase::Codegen,
        ));
    }

    Ok(prepared)
}

fn render_ptx_module(
    prepared: &[PreparedPtxShader],
    target_arch: ModuleArch,
) -> KainResult<String> {
    let mut output = String::new();
    output.push_str("// Generated by KAIN Compiler\n");
    output.push_str("// Raw PTX compute output; load with the CUDA Driver API.\n\n");
    output.push_str(&format!(".version {PTX_VERSION}\n"));
    output.push_str(&format!(".target {}\n", target_arch.as_sm()));
    output.push_str(".address_size 64\n\n");

    for shader in prepared {
        validate_prepared_shader(shader, target_arch)?;
        output.push_str(&shader.emitted_ptx);
        output.push('\n');
    }

    Ok(output)
}

fn resolve_variant_target_arches(
    options: PtxCodegenOptions,
    required_arch: ModuleArch,
    selection: PtxVariantSelection,
) -> KainResult<Vec<ModuleArch>> {
    if let Some(target_arch) = options.target_arch {
        validate_target_arch_floor(target_arch, required_arch)?;
        return Ok(vec![target_arch]);
    }

    let target_arches = match selection {
        PtxVariantSelection::PrimaryOnly => vec![options.resolve_module_target_arch(required_arch)],
        PtxVariantSelection::AutoFamily => PTX_ARCH_SPECS
            .iter()
            .map(|spec| spec.arch)
            .filter(|arch| *arch >= required_arch)
            .collect::<Vec<_>>(),
        PtxVariantSelection::Explicit(arches) => arches,
    };

    normalize_variant_target_arches(target_arches, required_arch)
}

fn normalize_variant_target_arches(
    mut arches: Vec<ModuleArch>,
    required_arch: ModuleArch,
) -> KainResult<Vec<ModuleArch>> {
    arches.sort_unstable();
    arches.dedup();

    if arches.is_empty() {
        arches.push(required_arch);
    }

    for arch in &arches {
        validate_target_arch_floor(*arch, required_arch)?;
    }

    Ok(arches)
}

fn validate_target_arch_floor(
    target_arch: ModuleArch,
    required_arch: ModuleArch,
) -> KainResult<()> {
    if target_arch < required_arch {
        return Err(KainError::codegen(
            format!(
                "PTX target {} is too old for this module and its required architecture floor {}",
                target_arch.as_sm(),
                required_arch.as_sm()
            ),
            Span::default(),
        ));
    }
    Ok(())
}

fn parse_target_arch_override_from_env() -> KainResult<Option<ModuleArch>> {
    if let Some(arch) = parse_single_target_arch_env(KAIN_PTX_ARCH_ENV)? {
        return Ok(Some(arch));
    }
    parse_single_target_arch_env(KAIN_CUDA_ARCH_ENV)
}

fn parse_single_target_arch_env(name: &str) -> KainResult<Option<ModuleArch>> {
    let Ok(raw) = std::env::var(name) else {
        return Ok(None);
    };
    ModuleArch::parse(&raw).map(Some).ok_or_else(|| {
        KainError::codegen(
            format!(
                "PTX backend received unsupported target arch '{}' from {}. Use one of {} or a compute capability like 7.5.",
                raw,
                name,
                ModuleArch::supported_target_examples()
            ),
            Span::default(),
        )
    })
}

fn validate_prepared_shader(shader: &PreparedPtxShader, target_arch: ModuleArch) -> KainResult<()> {
    let required_arch = shader.kernel_plan.minimum_arch();
    if target_arch < required_arch {
        return Err(KainError::codegen(
            format!(
                "PTX target {} is too old for kernel '{}' and its required architecture floor {}",
                target_arch.as_sm(),
                shader.name,
                required_arch.as_sm()
            ),
            shader.span,
        ));
    }

    shader.kernel_plan.validate(target_arch).map_err(|err| {
        KainError::codegen(
            format!(
                "PTX module validation failed for kernel '{}': {}",
                shader.name, err
            ),
            shader.span,
        )
    })
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
enum PtxScalarKind {
    U32,
    S32,
    F32,
    U64,
    Pred,
}

impl PtxScalarKind {
    fn op_suffix(self) -> &'static str {
        match self {
            Self::U32 => "u32",
            Self::S32 => "s32",
            Self::F32 => "f32",
            Self::U64 => "u64",
            Self::Pred => "pred",
        }
    }

    fn param_decl(self) -> &'static str {
        match self {
            Self::U32 | Self::Pred => ".u32",
            Self::S32 => ".s32",
            Self::F32 => ".f32",
            Self::U64 => ".u64",
        }
    }

    fn width_bits(self) -> u8 {
        match self {
            Self::Pred => 1,
            Self::U64 => 64,
            Self::U32 | Self::S32 | Self::F32 => 32,
        }
    }

    fn is_float(self) -> bool {
        matches!(self, Self::F32)
    }
}

#[derive(Clone, Debug)]
struct ScalarValue {
    reg: String,
    kind: PtxScalarKind,
}

#[derive(Clone, Debug)]
enum PtxValue {
    Scalar(ScalarValue),
    Vector(Vec<ScalarValue>),
    Void,
}

impl PtxValue {
    fn into_scalar(self, span: Span) -> KainResult<ScalarValue> {
        match self {
            PtxValue::Scalar(value) => Ok(value),
            PtxValue::Vector(_) => Err(KainError::codegen(
                "PTX backend expected a scalar expression but found a vector",
                span,
            )),
            PtxValue::Void => Err(KainError::codegen(
                "PTX backend expected a scalar expression but found void",
                span,
            )),
        }
    }
}

#[derive(Clone, Copy, Debug)]
enum BuiltinVector {
    DispatchThreadId,
    GroupThreadId,
    GroupId,
    BlockDim,
    GridDim,
}

#[derive(Clone, Copy, Debug)]
enum BuiltinScalar {
    GroupIndex,
}

#[derive(Clone, Debug)]
enum BindingValue {
    Scalar(ScalarValue),
    Vector(Vec<ScalarValue>),
    BuiltinVector(BuiltinVector),
    BuiltinScalar(BuiltinScalar),
    StorageBuffer { ptr: ScalarValue, elem_ty: Type },
    ConstU32(u32),
}

#[derive(Clone, Debug)]
struct PtxParam {
    name: String,
    kind: PtxScalarKind,
}

struct PtxContext {
    vars: HashMap<String, BindingValue>,
    params: Vec<PtxParam>,
    instructions: Vec<String>,
    shared_ops: Vec<ModuleSharedOpKind>,
    warp_ops: Vec<ModuleWarpOpKind>,
    tensor_ops: Vec<ModuleTensorOpRequest>,
    indent_level: usize,
    next_r: usize,
    next_rd: usize,
    next_f: usize,
    next_p: usize,
    next_label: usize,
    local_size: [u32; 3],
    exit_label: String,
    continue_labels: Vec<String>,
    break_labels: Vec<String>,
}

impl PtxContext {
    fn new(shader: &TypedShader) -> Self {
        let mut vars = HashMap::new();
        vars.insert(
            "dispatch_thread_id".to_string(),
            BindingValue::BuiltinVector(BuiltinVector::DispatchThreadId),
        );
        vars.insert(
            "global_invocation_id".to_string(),
            BindingValue::BuiltinVector(BuiltinVector::DispatchThreadId),
        );
        vars.insert(
            "group_thread_id".to_string(),
            BindingValue::BuiltinVector(BuiltinVector::GroupThreadId),
        );
        vars.insert(
            "local_invocation_id".to_string(),
            BindingValue::BuiltinVector(BuiltinVector::GroupThreadId),
        );
        vars.insert(
            "group_id".to_string(),
            BindingValue::BuiltinVector(BuiltinVector::GroupId),
        );
        vars.insert(
            "workgroup_id".to_string(),
            BindingValue::BuiltinVector(BuiltinVector::GroupId),
        );
        vars.insert(
            "block_idx".to_string(),
            BindingValue::BuiltinVector(BuiltinVector::GroupId),
        );
        vars.insert(
            "thread_idx".to_string(),
            BindingValue::BuiltinVector(BuiltinVector::GroupThreadId),
        );
        vars.insert(
            "block_dim".to_string(),
            BindingValue::BuiltinVector(BuiltinVector::BlockDim),
        );
        vars.insert(
            "grid_dim".to_string(),
            BindingValue::BuiltinVector(BuiltinVector::GridDim),
        );
        vars.insert(
            "group_index".to_string(),
            BindingValue::BuiltinScalar(BuiltinScalar::GroupIndex),
        );
        vars.insert(
            "local_invocation_index".to_string(),
            BindingValue::BuiltinScalar(BuiltinScalar::GroupIndex),
        );

        for input in &shader.ast.inputs {
            if is_uint_type(&input.ty) {
                vars.insert(
                    input.name.clone(),
                    BindingValue::BuiltinScalar(BuiltinScalar::GroupIndex),
                );
            } else {
                vars.insert(
                    input.name.clone(),
                    BindingValue::BuiltinVector(BuiltinVector::DispatchThreadId),
                );
            }
        }

        let local_size = shader.ast.workgroup_size.unwrap_or(DEFAULT_LOCAL_SIZE);

        Self {
            vars,
            params: Vec::new(),
            instructions: Vec::new(),
            shared_ops: Vec::new(),
            warp_ops: Vec::new(),
            tensor_ops: Vec::new(),
            indent_level: 1,
            next_r: 1,
            next_rd: 1,
            next_f: 1,
            next_p: 1,
            next_label: 0,
            local_size,
            exit_label: "$kain_exit".to_string(),
            continue_labels: Vec::new(),
            break_labels: Vec::new(),
        }
    }

    fn indent(&self) -> String {
        "    ".repeat(self.indent_level)
    }

    fn line(&mut self, line: impl Into<String>) {
        self.instructions
            .push(format!("{}{}", self.indent(), line.into()));
    }

    fn record_shared_op(&mut self, op: ModuleSharedOpKind) {
        self.shared_ops.push(op);
    }

    fn record_warp_op(&mut self, op: ModuleWarpOpKind) {
        self.warp_ops.push(op);
    }

    fn record_tensor_op(&mut self, op: ModuleTensorOpRequest) {
        self.tensor_ops.push(op);
    }

    fn label(&mut self, label: &str) {
        self.instructions.push(format!("{label}:"));
    }

    fn fresh_label(&mut self, prefix: &str) -> String {
        let label = format!("${prefix}_{}", self.next_label);
        self.next_label += 1;
        label
    }

    fn alloc_scalar(&mut self, kind: PtxScalarKind) -> ScalarValue {
        match kind {
            PtxScalarKind::U32 | PtxScalarKind::S32 => {
                let reg = format!("%r{}", self.next_r);
                self.next_r += 1;
                ScalarValue { reg, kind }
            }
            PtxScalarKind::F32 => {
                let reg = format!("%f{}", self.next_f);
                self.next_f += 1;
                ScalarValue { reg, kind }
            }
            PtxScalarKind::U64 => {
                let reg = format!("%rd{}", self.next_rd);
                self.next_rd += 1;
                ScalarValue { reg, kind }
            }
            PtxScalarKind::Pred => {
                let reg = format!("%p{}", self.next_p);
                self.next_p += 1;
                ScalarValue { reg, kind }
            }
        }
    }

    fn mov_u32_immediate(&mut self, value: u32) -> ScalarValue {
        let out = self.alloc_scalar(PtxScalarKind::U32);
        self.line(format!("mov.u32 {}, {};", out.reg, value));
        out
    }

    fn reg_declarations(&self) -> Vec<String> {
        let mut decls = Vec::new();
        if self.next_p > 1 {
            decls.push(format!("    .reg .pred %p<{}>;", self.next_p));
        }
        if self.next_r > 1 {
            decls.push(format!("    .reg .b32 %r<{}>;", self.next_r));
        }
        if self.next_rd > 1 {
            decls.push(format!("    .reg .b64 %rd<{}>;", self.next_rd));
        }
        if self.next_f > 1 {
            decls.push(format!("    .reg .f32 %f<{}>;", self.next_f));
        }
        decls
    }
}

#[cfg(test)]
fn emit_shader(shader: &TypedShader, target_arch: ModuleArch) -> KainResult<String> {
    let prepared = prepare_shader(shader)?;
    validate_prepared_shader(&prepared, target_arch)?;
    Ok(prepared.emitted_ptx)
}

/// Build an enriched `KainError` for a PTX codegen diagnostic.
///
/// Wraps a typed `DiagnosticReport` through the semantic enrichment
/// pipeline so the error carries a failure-mode classification, ranked
/// repairs, and a context-sensitive explanation instead of a bare string.
fn enriched_codegen_error(
    report: kain_core::error::DiagnosticReport,
    primary_text: &str,
    code: DiagnosticCode,
    phase: CompilerPhase,
) -> KainError {
    let packet = DiagnosticSemanticPacket::new(code, phase, primary_text);
    let enriched = enrich_semantic_report(report, &packet);
    KainError::rich(enriched)
}

fn prepare_shader(shader: &TypedShader) -> KainResult<PreparedPtxShader> {
    if shader.ast.stage != ShaderStage::Compute {
        let report = ShaderDiagnostic::stage_mismatch(format!(
            "PTX backend only supports compute shaders; shader '{}' uses {:?}",
            shader.ast.name, shader.ast.stage
        ))
        .primary_span(shader.ast.span)
        .help("Switch the shader stage to `compute` or move this kernel to the SPIR-V backend which supports vertex, fragment, and compute stages.");
        return Err(enriched_codegen_error(
            report,
            &shader.ast.name,
            DiagnosticCode::ShaderStageMismatch,
            CompilerPhase::Codegen,
        ));
    }

    validate_supported_shader(shader)?;

    let mut ctx = PtxContext::new(shader);
    emit_uniform_params(&mut ctx, shader)?;
    emit_block(&mut ctx, &shader.ast.body)?;
    ctx.label(&ctx.exit_label.clone());
    ctx.line("ret;");

    let mut output = String::new();
    let kernel_name = sanitize_ident(&shader.ast.name);
    let kernel_plan = build_kernel_plan(shader, &ctx, &kernel_name)?;
    output.push_str(&format!(".visible .entry {kernel_name}(\n"));
    for (index, param) in ctx.params.iter().enumerate() {
        let comma = if index + 1 == ctx.params.len() {
            ""
        } else {
            ","
        };
        output.push_str(&format!(
            "    .param {} {}{}\n",
            param.kind.param_decl(),
            param.name,
            comma
        ));
    }
    output.push_str(")\n{\n");
    for decl in ctx.reg_declarations() {
        output.push_str(&decl);
        output.push('\n');
    }
    if !ctx.instructions.is_empty() && !ctx.reg_declarations().is_empty() {
        output.push('\n');
    }
    for line in ctx.instructions {
        output.push_str(&line);
        output.push('\n');
    }
    output.push_str("}\n");

    Ok(PreparedPtxShader {
        name: shader.ast.name.clone(),
        span: shader.ast.span,
        emitted_ptx: output,
        kernel_plan,
    })
}

fn build_kernel_plan(
    shader: &TypedShader,
    ctx: &PtxContext,
    kernel_name: &str,
) -> KainResult<ModuleKernelPlan> {
    let mut plan = ModuleKernelPlan::new(
        shader.ast.name.clone(),
        kernel_name.to_string(),
        ModuleLaunchConfig::new([1, 1, 1], ctx.local_size, 0),
    );

    let mut uniforms = shader.ast.uniforms.iter().collect::<Vec<_>>();
    uniforms.sort_by_key(|uniform| uniform.binding);

    for uniform in uniforms {
        if is_local_size_param(&uniform.name) {
            continue;
        }

        let param_name = format!("_kain_param_{}", sanitize_ident(&uniform.name));
        if is_storage_buffer(&uniform.ty) {
            plan.add_param(ModuleKernelParam::pointer(
                param_name,
                ModuleAddressSpace::Global,
            ));
            continue;
        }

        let Some(shape) = type_shape(&uniform.ty) else {
            return Err(KainError::codegen(
                format!(
                    "PTX module plan cannot describe uniform '{}' of type {}",
                    uniform.name,
                    type_name(&uniform.ty)
                ),
                uniform.span,
            ));
        };

        if shape.lanes == 1 {
            plan.add_param(ModuleKernelParam::scalar(
                param_name,
                module_scalar_kind(shape.scalar),
            ));
            continue;
        }

        for lane in 0..shape.lanes {
            plan.add_param(ModuleKernelParam::scalar(
                format!("{}_{}", param_name, lane_name(lane)),
                module_scalar_kind(shape.scalar),
            ));
        }
    }

    if plan.params.len() != ctx.params.len() {
        return Err(KainError::codegen(
            format!(
                "PTX module plan drifted from emitted parameter count for kernel '{}': planned {}, emitted {}",
                shader.ast.name,
                plan.params.len(),
                ctx.params.len()
            ),
            shader.ast.span,
        ));
    }

    for (emitted, planned) in ctx.params.iter().zip(&plan.params) {
        let Some(planned_kind) = emitted_kind_for_module_param(planned) else {
            return Err(KainError::codegen(
                format!(
                    "PTX module plan produced a parameter kind the live emitter does not lower yet for '{}'",
                    planned.name
                ),
                shader.ast.span,
            ));
        };
        if emitted.name != planned.name || emitted.kind != planned_kind {
            return Err(KainError::codegen(
                format!(
                    "PTX module plan drifted from emitted ABI for kernel '{}': emitted '{}' as {}, planned '{}' as {}",
                    shader.ast.name,
                    emitted.name,
                    emitted.kind.op_suffix(),
                    planned.name,
                    module_param_label(planned)
                ),
                shader.ast.span,
            ));
        }
    }

    for op in &ctx.shared_ops {
        plan.add_shared_op(*op);
    }
    for op in &ctx.warp_ops {
        plan.add_warp_op(*op);
    }
    for op in &ctx.tensor_ops {
        plan.add_tensor_op(*op);
    }

    Ok(plan)
}

fn emit_uniform_params(ctx: &mut PtxContext, shader: &TypedShader) -> KainResult<()> {
    let mut uniforms = shader.ast.uniforms.iter().collect::<Vec<_>>();
    uniforms.sort_by_key(|uniform| uniform.binding);

    for uniform in uniforms {
        if is_local_size_param(&uniform.name) {
            ctx.vars.insert(
                uniform.name.clone(),
                BindingValue::ConstU32(match uniform.name.as_str() {
                    "LOCAL_SIZE_X" => ctx.local_size[0],
                    "LOCAL_SIZE_Y" => ctx.local_size[1],
                    "LOCAL_SIZE_Z" => ctx.local_size[2],
                    _ => 1,
                }),
            );
            continue;
        }

        let param_name = format!("_kain_param_{}", sanitize_ident(&uniform.name));
        if is_storage_buffer(&uniform.ty) {
            ctx.params.push(PtxParam {
                name: param_name.clone(),
                kind: PtxScalarKind::U64,
            });
            let ptr = ctx.alloc_scalar(PtxScalarKind::U64);
            ctx.line(format!("ld.param.u64 {}, [{}];", ptr.reg, param_name));
            ctx.vars.insert(
                uniform.name.clone(),
                BindingValue::StorageBuffer {
                    ptr,
                    elem_ty: storage_buffer_elem_type(&uniform.ty, uniform.span),
                },
            );
        } else if let Some(shape) = type_shape(&uniform.ty) {
            if shape.lanes == 1 {
                let kind = shape.scalar;
                ctx.params.push(PtxParam {
                    name: param_name.clone(),
                    kind,
                });
                let value = ctx.alloc_scalar(kind);
                ctx.line(format!(
                    "ld.param.{} {}, [{}];",
                    kind.op_suffix(),
                    value.reg,
                    param_name
                ));
                ctx.vars
                    .insert(uniform.name.clone(), BindingValue::Scalar(value));
            } else {
                let mut values = Vec::with_capacity(shape.lanes);
                for lane in 0..shape.lanes {
                    let lane_param = format!("{}_{}", param_name, lane_name(lane));
                    ctx.params.push(PtxParam {
                        name: lane_param.clone(),
                        kind: shape.scalar,
                    });
                    let value = ctx.alloc_scalar(shape.scalar);
                    ctx.line(format!(
                        "ld.param.{} {}, [{}];",
                        shape.scalar.op_suffix(),
                        value.reg,
                        lane_param
                    ));
                    values.push(value);
                }
                ctx.vars
                    .insert(uniform.name.clone(), BindingValue::Vector(values));
            }
        } else {
            return Err(KainError::codegen(
                format!(
                    "PTX backend does not support uniform '{}' of type {}",
                    uniform.name,
                    type_name(&uniform.ty)
                ),
                uniform.span,
            ));
        }
    }

    Ok(())
}

fn emit_block(ctx: &mut PtxContext, block: &Block) -> KainResult<()> {
    for stmt in &block.stmts {
        emit_stmt(ctx, stmt)?;
    }
    Ok(())
}

fn emit_stmt(ctx: &mut PtxContext, stmt: &Stmt) -> KainResult<()> {
    match stmt {
        Stmt::Let {
            pattern,
            ty,
            value,
            span,
        } => {
            let Pattern::Binding { name, .. } = pattern else {
                return Err(KainError::codegen(
                    "PTX backend only supports simple let bindings in compute shaders",
                    *span,
                ));
            };
            let value = if let Some(value) = value {
                emit_expr(ctx, value)?
            } else if let Some(ty) = ty {
                zero_value_for_type(ctx, ty, *span)?
            } else {
                return Err(KainError::codegen(
                    "PTX backend needs either a value or an explicit type for let bindings",
                    *span,
                ));
            };
            bind_value(ctx, name, value, *span)?;
        }
        Stmt::Expr(Expr::If {
            condition,
            then_branch,
            else_branch,
            ..
        }) => emit_if_stmt(ctx, condition, then_branch, else_branch.as_deref())?,
        Stmt::Expr(expr) => {
            let _ = emit_expr(ctx, expr)?;
        }
        Stmt::Defer { span, .. } => {
            return Err(KainError::codegen(
                "PTX backend does not support defer inside compute shaders",
                *span,
            ));
        }
        Stmt::Dispatch { span, .. } => {
            return Err(KainError::codegen(
                "PTX backend does not support host dispatch statements inside compute shaders",
                *span,
            ));
        }
        Stmt::Return(value, _) => {
            if let Some(value) = value {
                let _ = emit_expr(ctx, value)?;
            }
            ctx.line(format!("bra {};", ctx.exit_label));
        }
        Stmt::While {
            condition,
            body,
            span: _,
        } => emit_while_stmt(ctx, condition, body)?,
        Stmt::Break(_, span) => {
            let Some(label) = ctx.break_labels.last() else {
                return Err(KainError::codegen("break used outside a PTX loop", *span));
            };
            ctx.line(format!("bra {label};"));
        }
        Stmt::Continue(span) => {
            let Some(label) = ctx.continue_labels.last() else {
                return Err(KainError::codegen(
                    "continue used outside a PTX loop",
                    *span,
                ));
            };
            ctx.line(format!("bra {label};"));
        }
        Stmt::For { span, .. } | Stmt::Loop { span, .. } => {
            return Err(KainError::codegen(
                "PTX backend v1 supports while loops but not for/loop lowering yet",
                *span,
            ));
        }
        Stmt::Fanout { span, .. } => {
            return Err(KainError::codegen(
                "PTX backend does not support shared fanout lowering",
                *span,
            ));
        }
        Stmt::Item(_) => {
            // Shader-local comptime metadata is consumed before PTX lowering.
            // Ignore nested items here rather than rejecting otherwise valid
            // compute kernels that carry residency/planning sidecars.
        }
    }
    Ok(())
}

fn emit_if_stmt(
    ctx: &mut PtxContext,
    condition: &Expr,
    then_branch: &Block,
    else_branch: Option<&ElseBranch>,
) -> KainResult<()> {
    let pred = emit_condition(ctx, condition)?;
    let else_label = ctx.fresh_label("if_else");
    let end_label = ctx.fresh_label("if_end");

    ctx.line(format!("@!{} bra {};", pred.reg, else_label));
    emit_block(ctx, then_branch)?;
    ctx.line(format!("bra {};", end_label));
    ctx.label(&else_label);
    if let Some(branch) = else_branch {
        emit_else_branch(ctx, branch)?;
    }
    ctx.label(&end_label);
    Ok(())
}

fn emit_else_branch(ctx: &mut PtxContext, branch: &ElseBranch) -> KainResult<()> {
    match branch {
        ElseBranch::Else(block) => emit_block(ctx, block),
        ElseBranch::ElseIf(condition, block, next) => {
            emit_if_stmt(ctx, condition, block, next.as_deref())
        }
    }
}

fn emit_while_stmt(ctx: &mut PtxContext, condition: &Expr, body: &Block) -> KainResult<()> {
    let loop_label = ctx.fresh_label("while_cond");
    let end_label = ctx.fresh_label("while_end");
    ctx.continue_labels.push(loop_label.clone());
    ctx.break_labels.push(end_label.clone());

    ctx.label(&loop_label);
    let pred = emit_condition(ctx, condition)?;
    ctx.line(format!("@!{} bra {};", pred.reg, end_label));
    emit_block(ctx, body)?;
    ctx.line(format!("bra {};", loop_label));
    ctx.label(&end_label);

    ctx.continue_labels.pop();
    ctx.break_labels.pop();
    Ok(())
}

fn bind_value(ctx: &mut PtxContext, name: &str, value: PtxValue, span: Span) -> KainResult<()> {
    let binding = match value {
        PtxValue::Scalar(value) => BindingValue::Scalar(value),
        PtxValue::Vector(value) => BindingValue::Vector(value),
        PtxValue::Void => {
            return Err(KainError::codegen(
                format!("PTX backend cannot bind void expression to '{name}'"),
                span,
            ));
        }
    };
    ctx.vars.insert(name.to_string(), binding);
    Ok(())
}

fn emit_expr(ctx: &mut PtxContext, expr: &Expr) -> KainResult<PtxValue> {
    match expr {
        Expr::Int(value, _) => {
            let out = ctx.alloc_scalar(PtxScalarKind::S32);
            ctx.line(format!("mov.s32 {}, {};", out.reg, *value as i32));
            Ok(PtxValue::Scalar(out))
        }
        Expr::Float(value, _) => {
            let out = ctx.alloc_scalar(PtxScalarKind::F32);
            let bits = (*value as f32).to_bits();
            ctx.line(format!("mov.f32 {}, 0f{bits:08x};", out.reg));
            Ok(PtxValue::Scalar(out))
        }
        Expr::Bool(value, _) => {
            let out = ctx.alloc_scalar(PtxScalarKind::U32);
            ctx.line(format!("mov.u32 {}, {};", out.reg, u32::from(*value)));
            Ok(PtxValue::Scalar(out))
        }
        Expr::Ident(name, span) => emit_ident(ctx, name, *span),
        Expr::Field {
            object,
            field,
            span,
        } => emit_field(ctx, object, field, *span),
        Expr::Index {
            object,
            index,
            span,
        } => emit_index_load(ctx, object, index, *span),
        Expr::Assign {
            target,
            value,
            span,
        } => emit_assignment(ctx, target, value, *span),
        Expr::Binary {
            left,
            op,
            right,
            span,
        } => emit_binary(ctx, left, *op, right, *span),
        Expr::Unary { op, operand, span } => emit_unary(ctx, *op, operand, *span),
        Expr::Call { callee, args, span } => {
            let Expr::Ident(name, _) = callee.as_ref() else {
                return Err(KainError::codegen(
                    "PTX backend only supports direct function and constructor calls",
                    *span,
                ));
            };
            emit_call(ctx, name, args, *span)
        }
        Expr::Cast {
            value,
            target,
            span,
        } => {
            let value = emit_expr(ctx, value)?.into_scalar(*span)?;
            let Some(kind) = scalar_kind_for_type(target) else {
                return Err(KainError::codegen(
                    format!("PTX backend cannot cast to {}", type_name(target)),
                    *span,
                ));
            };
            Ok(PtxValue::Scalar(coerce_scalar(ctx, value, kind)?))
        }
        Expr::Paren(inner, _) => emit_expr(ctx, inner),
        Expr::If {
            condition,
            then_branch,
            else_branch,
            span,
        } => {
            emit_if_stmt(ctx, condition, then_branch, else_branch.as_deref())?;
            Err(KainError::codegen(
                "PTX backend v1 supports if statements, not value-producing if expressions",
                *span,
            ))
        }
        Expr::Return(value, _) => {
            if let Some(value) = value {
                let _ = emit_expr(ctx, value)?;
            }
            ctx.line(format!("bra {};", ctx.exit_label));
            Ok(PtxValue::Void)
        }
        Expr::Block(block, _) => {
            emit_block(ctx, block)?;
            Ok(PtxValue::Void)
        }
        _ => Err(KainError::codegen(
            "PTX backend does not support this expression in v1",
            expr.span(),
        )),
    }
}

fn emit_ident(ctx: &mut PtxContext, name: &str, span: Span) -> KainResult<PtxValue> {
    let Some(binding) = ctx.vars.get(name).cloned() else {
        return Err(KainError::codegen(
            format!("PTX backend could not resolve identifier '{name}'"),
            span,
        ));
    };
    match binding {
        BindingValue::Scalar(value) => Ok(PtxValue::Scalar(value)),
        BindingValue::Vector(value) => Ok(PtxValue::Vector(value)),
        BindingValue::BuiltinScalar(BuiltinScalar::GroupIndex) => {
            Ok(PtxValue::Scalar(emit_group_index(ctx)))
        }
        BindingValue::ConstU32(value) => Ok(PtxValue::Scalar(ctx.mov_u32_immediate(value))),
        BindingValue::BuiltinVector(_) => Err(KainError::codegen(
            format!("PTX backend needs a field access for vector builtin '{name}'"),
            span,
        )),
        BindingValue::StorageBuffer { .. } => Err(KainError::codegen(
            format!("PTX backend needs an index access for storage buffer '{name}'"),
            span,
        )),
    }
}

fn emit_field(
    ctx: &mut PtxContext,
    object: &Expr,
    field: &str,
    span: Span,
) -> KainResult<PtxValue> {
    let lane = vector_lane(field).ok_or_else(|| {
        KainError::codegen(
            format!("PTX backend only supports x/y/z/w vector fields, got '.{field}'"),
            span,
        )
    })?;

    if let Expr::Ident(name, _) = object {
        if let Some(binding) = ctx.vars.get(name).cloned() {
            match binding {
                BindingValue::BuiltinVector(kind) => {
                    return Ok(PtxValue::Scalar(emit_builtin_vector_component(
                        ctx, kind, lane,
                    )));
                }
                BindingValue::Vector(values) => {
                    return values
                        .get(lane)
                        .cloned()
                        .map(PtxValue::Scalar)
                        .ok_or_else(|| {
                            KainError::codegen(
                                format!("PTX vector field '.{field}' is out of range"),
                                span,
                            )
                        });
                }
                _ => {}
            }
        }
    }

    match emit_expr(ctx, object)? {
        PtxValue::Vector(values) => values
            .get(lane)
            .cloned()
            .map(PtxValue::Scalar)
            .ok_or_else(|| KainError::codegen("PTX vector field is out of range", span)),
        _ => Err(KainError::codegen(
            "PTX field access is only supported for vector values and compute builtins",
            span,
        )),
    }
}

fn emit_index_load(
    ctx: &mut PtxContext,
    object: &Expr,
    index: &Expr,
    span: Span,
) -> KainResult<PtxValue> {
    let Expr::Ident(name, _) = object else {
        return Err(KainError::codegen(
            "PTX backend only supports direct storage-buffer indexing",
            span,
        ));
    };
    let Some(BindingValue::StorageBuffer { ptr, elem_ty }) = ctx.vars.get(name).cloned() else {
        return Err(KainError::codegen(
            format!("PTX backend can only index storage buffers, got '{name}'"),
            span,
        ));
    };

    let index = emit_expr(ctx, index)?.into_scalar(span)?;
    let index = coerce_scalar(ctx, index, PtxScalarKind::U32)?;
    let elem_shape = type_shape(&elem_ty).ok_or_else(|| {
        KainError::codegen(
            format!(
                "PTX backend does not support storage-buffer element type {}",
                type_name(&elem_ty)
            ),
            span,
        )
    })?;

    if elem_shape.lanes == 1 {
        let address = emit_storage_address(ctx, &ptr, &index, elem_shape.stride, 0);
        let out = ctx.alloc_scalar(elem_shape.scalar);
        ctx.line(format!(
            "ld.global.{} {}, [{}];",
            elem_shape.load_suffix, out.reg, address.reg
        ));
        Ok(PtxValue::Scalar(out))
    } else {
        let mut lanes = Vec::with_capacity(elem_shape.lanes);
        for lane in 0..elem_shape.lanes {
            let address = emit_storage_address(
                ctx,
                &ptr,
                &index,
                elem_shape.stride,
                (lane as u32) * elem_shape.component_stride,
            );
            let out = ctx.alloc_scalar(elem_shape.scalar);
            ctx.line(format!(
                "ld.global.{} {}, [{}];",
                elem_shape.load_suffix, out.reg, address.reg
            ));
            lanes.push(out);
        }
        Ok(PtxValue::Vector(lanes))
    }
}

fn emit_assignment(
    ctx: &mut PtxContext,
    target: &Expr,
    value: &Expr,
    span: Span,
) -> KainResult<PtxValue> {
    match target {
        Expr::Ident(name, _) => {
            let value = emit_expr(ctx, value)?;
            if let Some(existing) = ctx.vars.get(name).cloned() {
                assign_existing_binding(ctx, name, existing, value, span)?;
            } else {
                bind_value(ctx, name, value, span)?;
            }
            Ok(PtxValue::Void)
        }
        Expr::Index { object, index, .. } => {
            emit_index_store(ctx, object, index, value, span)?;
            Ok(PtxValue::Void)
        }
        _ => Err(KainError::codegen(
            "PTX backend only supports assignment to locals and storage-buffer indices",
            span,
        )),
    }
}

fn assign_existing_binding(
    ctx: &mut PtxContext,
    name: &str,
    existing: BindingValue,
    value: PtxValue,
    span: Span,
) -> KainResult<()> {
    match existing {
        BindingValue::Scalar(slot) => {
            let value = value.into_scalar(span)?;
            let value = coerce_scalar(ctx, value, slot.kind)?;
            if value.reg != slot.reg {
                ctx.line(format!(
                    "mov.{} {}, {};",
                    slot.kind.op_suffix(),
                    slot.reg,
                    value.reg
                ));
            }
            ctx.vars
                .insert(name.to_string(), BindingValue::Scalar(slot));
            Ok(())
        }
        BindingValue::Vector(slots) => {
            let values = match value {
                PtxValue::Vector(values) if values.len() == slots.len() => values,
                _ => {
                    return Err(KainError::codegen(
                        format!(
                            "PTX backend cannot assign a non-matching value shape to vector local '{name}'"
                        ),
                        span,
                    ))
                }
            };
            for (slot, value) in slots.iter().zip(values.into_iter()) {
                let value = coerce_scalar(ctx, value, slot.kind)?;
                if value.reg != slot.reg {
                    ctx.line(format!(
                        "mov.{} {}, {};",
                        slot.kind.op_suffix(),
                        slot.reg,
                        value.reg
                    ));
                }
            }
            ctx.vars
                .insert(name.to_string(), BindingValue::Vector(slots));
            Ok(())
        }
        BindingValue::StorageBuffer { .. } => Err(KainError::codegen(
            format!("PTX backend cannot assign a new value to storage buffer '{name}'"),
            span,
        )),
        BindingValue::BuiltinVector(_)
        | BindingValue::BuiltinScalar(_)
        | BindingValue::ConstU32(_) => Err(KainError::codegen(
            format!("PTX backend cannot assign to compiler-owned binding '{name}'"),
            span,
        )),
    }
}

fn emit_index_store(
    ctx: &mut PtxContext,
    object: &Expr,
    index: &Expr,
    value: &Expr,
    span: Span,
) -> KainResult<()> {
    let Expr::Ident(name, _) = object else {
        return Err(KainError::codegen(
            "PTX backend only supports direct storage-buffer stores",
            span,
        ));
    };
    let Some(BindingValue::StorageBuffer { ptr, elem_ty }) = ctx.vars.get(name).cloned() else {
        return Err(KainError::codegen(
            format!("PTX backend can only store into storage buffers, got '{name}'"),
            span,
        ));
    };
    let index = emit_expr(ctx, index)?.into_scalar(span)?;
    let index = coerce_scalar(ctx, index, PtxScalarKind::U32)?;
    let elem_shape = type_shape(&elem_ty).ok_or_else(|| {
        KainError::codegen(
            format!(
                "PTX backend does not support storage-buffer element type {}",
                type_name(&elem_ty)
            ),
            span,
        )
    })?;
    let value = emit_expr(ctx, value)?;

    match value {
        PtxValue::Scalar(value) if elem_shape.lanes == 1 => {
            let value = coerce_scalar(ctx, value, elem_shape.scalar)?;
            let address = emit_storage_address(ctx, &ptr, &index, elem_shape.stride, 0);
            ctx.line(format!(
                "st.global.{} [{}], {};",
                elem_shape.store_suffix, address.reg, value.reg
            ));
        }
        PtxValue::Vector(values) if values.len() == elem_shape.lanes => {
            for (lane, value) in values.into_iter().enumerate() {
                let value = coerce_scalar(ctx, value, elem_shape.scalar)?;
                let address = emit_storage_address(
                    ctx,
                    &ptr,
                    &index,
                    elem_shape.stride,
                    (lane as u32) * elem_shape.component_stride,
                );
                ctx.line(format!(
                    "st.global.{} [{}], {};",
                    elem_shape.store_suffix, address.reg, value.reg
                ));
            }
        }
        _ => {
            return Err(KainError::codegen(
                "PTX storage-buffer store value does not match the element shape",
                span,
            ));
        }
    }

    Ok(())
}

fn emit_binary(
    ctx: &mut PtxContext,
    left: &Expr,
    op: BinaryOp,
    right: &Expr,
    span: Span,
) -> KainResult<PtxValue> {
    let left_value = emit_expr(ctx, left)?;
    let right_value = emit_expr(ctx, right)?;

    match (left_value, right_value) {
        (PtxValue::Scalar(left), PtxValue::Scalar(right)) => Ok(PtxValue::Scalar(
            emit_binary_scalar(ctx, left, op, right, span)?,
        )),
        (PtxValue::Vector(left), PtxValue::Vector(right)) if left.len() == right.len() => {
            let mut values = Vec::with_capacity(left.len());
            for (left, right) in left.into_iter().zip(right.into_iter()) {
                values.push(emit_binary_scalar(ctx, left, op, right, span)?);
            }
            Ok(PtxValue::Vector(values))
        }
        _ => Err(KainError::codegen(
            "PTX backend only supports scalar-scalar or same-size vector-vector binary operations",
            span,
        )),
    }
}

fn emit_binary_scalar(
    ctx: &mut PtxContext,
    left: ScalarValue,
    op: BinaryOp,
    right: ScalarValue,
    span: Span,
) -> KainResult<ScalarValue> {
    match op {
        BinaryOp::Eq | BinaryOp::Ne | BinaryOp::Lt | BinaryOp::Le | BinaryOp::Gt | BinaryOp::Ge => {
            emit_compare(ctx, left, op, right, span)
        }
        BinaryOp::And | BinaryOp::Or => emit_logical(ctx, left, op, right),
        BinaryOp::Add
        | BinaryOp::Sub
        | BinaryOp::Mul
        | BinaryOp::Div
        | BinaryOp::Mod
        | BinaryOp::BitAnd
        | BinaryOp::BitOr
        | BinaryOp::BitXor
        | BinaryOp::Shl
        | BinaryOp::Shr => emit_arithmetic(ctx, left, op, right, span),
        _ => Err(KainError::codegen(
            "PTX backend does not support this binary operator",
            span,
        )),
    }
}

fn emit_arithmetic(
    ctx: &mut PtxContext,
    left: ScalarValue,
    op: BinaryOp,
    right: ScalarValue,
    span: Span,
) -> KainResult<ScalarValue> {
    let kind = arithmetic_kind(left.kind, right.kind, span)?;
    let left = coerce_scalar(ctx, left, kind)?;
    let right = coerce_scalar(ctx, right, kind)?;
    let out = ctx.alloc_scalar(kind);

    let def = binary_op_inst(op, kind).ok_or_else(|| {
        KainError::codegen(
            "PTX backend does not support this arithmetic/type combination",
            span,
        )
    })?;
    emit_inst(ctx, def.mnemonic, def.suffix, &out, &[&left, &right]);
    Ok(out)
}

fn emit_compare(
    ctx: &mut PtxContext,
    left: ScalarValue,
    op: BinaryOp,
    right: ScalarValue,
    span: Span,
) -> KainResult<ScalarValue> {
    let kind = arithmetic_kind(left.kind, right.kind, span)?;
    let left = coerce_scalar(ctx, left, kind)?;
    let right = coerce_scalar(ctx, right, kind)?;
    let out = ctx.alloc_scalar(PtxScalarKind::Pred);
    let cmp = cmp_inst(op)
        .ok_or_else(|| KainError::codegen("PTX backend does not support this comparison", span))?;
    ctx.line(format!(
        "{cmp}.{} {}, {}, {};",
        kind.op_suffix(),
        out.reg,
        left.reg,
        right.reg
    ));
    Ok(out)
}

fn emit_logical(
    ctx: &mut PtxContext,
    left: ScalarValue,
    op: BinaryOp,
    right: ScalarValue,
) -> KainResult<ScalarValue> {
    let left = scalar_to_pred(ctx, left);
    let right = scalar_to_pred(ctx, right);
    let out = ctx.alloc_scalar(PtxScalarKind::Pred);
    let instr = match op {
        BinaryOp::And => "and.pred",
        BinaryOp::Or => "or.pred",
        _ => unreachable!(),
    };
    ctx.line(format!("{instr} {}, {}, {};", out.reg, left.reg, right.reg));
    Ok(out)
}

fn emit_unary(
    ctx: &mut PtxContext,
    op: UnaryOp,
    operand: &Expr,
    span: Span,
) -> KainResult<PtxValue> {
    let value = emit_expr(ctx, operand)?.into_scalar(span)?;
    match op {
        UnaryOp::Neg => {
            let out = ctx.alloc_scalar(value.kind);
            let instr = match value.kind {
                PtxScalarKind::F32 => format!("neg.f32 {}, {};", out.reg, value.reg),
                PtxScalarKind::S32 | PtxScalarKind::U32 => {
                    format!("neg.s32 {}, {};", out.reg, value.reg)
                }
                PtxScalarKind::U64 => {
                    return Err(KainError::codegen(
                        "PTX backend does not support unary negation for 64-bit integers",
                        span,
                    ));
                }
                PtxScalarKind::Pred => {
                    return Err(KainError::codegen(
                        "PTX backend does not support unary negation for predicates",
                        span,
                    ));
                }
            };
            ctx.line(instr);
            Ok(PtxValue::Scalar(out))
        }
        UnaryOp::Not => {
            let pred = scalar_to_pred(ctx, value);
            let out = ctx.alloc_scalar(PtxScalarKind::Pred);
            ctx.line(format!("not.pred {}, {};", out.reg, pred.reg));
            Ok(PtxValue::Scalar(out))
        }
        UnaryOp::BitNot => {
            let target = if value.kind == PtxScalarKind::U64 {
                PtxScalarKind::U64
            } else {
                PtxScalarKind::U32
            };
            let value = coerce_scalar(ctx, value, target)?;
            let out = ctx.alloc_scalar(target);
            ctx.line(format!(
                "not.b{} {}, {};",
                target.width_bits(),
                out.reg,
                value.reg
            ));
            Ok(PtxValue::Scalar(out))
        }
        UnaryOp::Ref | UnaryOp::RefMut | UnaryOp::Deref => Err(KainError::codegen(
            "PTX backend does not support reference or dereference expressions in v1",
            span,
        )),
    }
}

fn emit_call(
    ctx: &mut PtxContext,
    name: &str,
    args: &[CallArg],
    span: Span,
) -> KainResult<PtxValue> {
    if let Some(value) = emit_cuda_intrinsic_call(ctx, name, args, span)? {
        return Ok(value);
    }

    match name {
        "max" if args.len() == 2 => emit_min_max_call(ctx, args, span, true),
        "min" if args.len() == 2 => emit_min_max_call(ctx, args, span, false),
        "Float" | "float" if args.len() == 1 => {
            let value = emit_expr(ctx, &args[0].value)?.into_scalar(span)?;
            Ok(PtxValue::Scalar(coerce_scalar(
                ctx,
                value,
                PtxScalarKind::F32,
            )?))
        }
        "Int" | "int" if args.len() == 1 => {
            let value = emit_expr(ctx, &args[0].value)?.into_scalar(span)?;
            Ok(PtxValue::Scalar(coerce_scalar(
                ctx,
                value,
                PtxScalarKind::S32,
            )?))
        }
        "UInt" | "uint" if args.len() == 1 => {
            let value = emit_expr(ctx, &args[0].value)?.into_scalar(span)?;
            Ok(PtxValue::Scalar(coerce_scalar(
                ctx,
                value,
                PtxScalarKind::U32,
            )?))
        }
        "UInt64" | "uint64" | "u64" if args.len() == 1 => {
            let value = emit_expr(ctx, &args[0].value)?.into_scalar(span)?;
            Ok(PtxValue::Scalar(coerce_scalar(
                ctx,
                value,
                PtxScalarKind::U64,
            )?))
        }
        "Bool" | "bool" if args.len() == 1 => {
            let value = emit_expr(ctx, &args[0].value)?.into_scalar(span)?;
            let pred = scalar_to_pred(ctx, value);
            let out = coerce_scalar(ctx, pred, PtxScalarKind::U32)?;
            Ok(PtxValue::Scalar(out))
        }
        "Vec2" | "vec2" | "Vec3" | "vec3" | "Vec4" | "vec4" | "IVec2" | "ivec2" | "IVec3"
        | "ivec3" | "IVec4" | "ivec4" | "UVec2" | "uvec2" | "UVec3" | "uvec3" | "UVec4"
        | "uvec4" => emit_vector_constructor(ctx, name, args, span),
        _ => Err(KainError::codegen(
            format!("PTX backend does not support function '{name}' in v1"),
            span,
        )),
    }
}

fn emit_cuda_intrinsic_call(
    ctx: &mut PtxContext,
    name: &str,
    args: &[CallArg],
    span: Span,
) -> KainResult<Option<PtxValue>> {
    match name {
        "cuda_lane_id" => {
            expect_cuda_arg_count(name, args, 0, span)?;
            let out = ctx.alloc_scalar(PtxScalarKind::U32);
            ctx.line(format!("mov.u32 {}, %laneid;", out.reg));
            Ok(Some(PtxValue::Scalar(out)))
        }
        "cuda_warp_id" => {
            expect_cuda_arg_count(name, args, 0, span)?;
            let out = ctx.alloc_scalar(PtxScalarKind::U32);
            ctx.line(format!("mov.u32 {}, %warpid;", out.reg));
            Ok(Some(PtxValue::Scalar(out)))
        }
        "cuda_active_mask" => {
            expect_cuda_arg_count(name, args, 0, span)?;
            ctx.record_warp_op(ModuleWarpOpKind::Activemask);
            let out = ctx.alloc_scalar(PtxScalarKind::U32);
            ctx.line(format!("activemask.b32 {};", out.reg));
            Ok(Some(PtxValue::Scalar(out)))
        }
        "cuda_block_sync" | "cuda_barrier_sync" => {
            expect_cuda_arg_count(name, args, 0, span)?;
            ctx.record_shared_op(ModuleSharedOpKind::BarSync);
            ctx.line("bar.sync 0;");
            Ok(Some(PtxValue::Void))
        }
        "cuda_warp_sync" => {
            expect_cuda_arg_count(name, args, 1, span)?;
            ctx.record_warp_op(ModuleWarpOpKind::BarWarpSync);
            let mask = emit_expr(ctx, &args[0].value)?.into_scalar(span)?;
            let mask = coerce_scalar(ctx, mask, PtxScalarKind::U32)?;
            ctx.line(format!("bar.warp.sync {};", mask.reg));
            Ok(Some(PtxValue::Void))
        }
        "cuda_ballot" => {
            expect_cuda_arg_count(name, args, 1, span)?;
            ctx.record_warp_op(ModuleWarpOpKind::BallotSync);
            let pred = emit_expr(ctx, &args[0].value)?.into_scalar(span)?;
            let pred = scalar_to_pred(ctx, pred);
            let mask = ctx.mov_u32_immediate(u32::MAX);
            let out = ctx.alloc_scalar(PtxScalarKind::U32);
            ctx.line(format!(
                "vote.sync.ballot.b32 {}, {}, {};",
                out.reg, pred.reg, mask.reg
            ));
            Ok(Some(PtxValue::Scalar(out)))
        }
        "cuda_warp_any" => {
            expect_cuda_arg_count(name, args, 1, span)?;
            ctx.record_warp_op(ModuleWarpOpKind::AnySync);
            let pred = emit_expr(ctx, &args[0].value)?.into_scalar(span)?;
            let pred = scalar_to_pred(ctx, pred);
            let mask = ctx.mov_u32_immediate(u32::MAX);
            let out = ctx.alloc_scalar(PtxScalarKind::Pred);
            ctx.line(format!(
                "vote.sync.any.pred {}, {}, {};",
                out.reg, pred.reg, mask.reg
            ));
            Ok(Some(PtxValue::Scalar(out)))
        }
        "cuda_warp_all" => {
            expect_cuda_arg_count(name, args, 1, span)?;
            ctx.record_warp_op(ModuleWarpOpKind::AllSync);
            let pred = emit_expr(ctx, &args[0].value)?.into_scalar(span)?;
            let pred = scalar_to_pred(ctx, pred);
            let mask = ctx.mov_u32_immediate(u32::MAX);
            let out = ctx.alloc_scalar(PtxScalarKind::Pred);
            ctx.line(format!(
                "vote.sync.all.pred {}, {}, {};",
                out.reg, pred.reg, mask.reg
            ));
            Ok(Some(PtxValue::Scalar(out)))
        }
        "cuda_shfl_xor_u32" => {
            expect_cuda_arg_count(name, args, 2, span)?;
            let value = emit_expr(ctx, &args[0].value)?.into_scalar(span)?;
            let value = coerce_scalar(ctx, value, PtxScalarKind::U32)?;
            let lane_mask = emit_expr(ctx, &args[1].value)?.into_scalar(span)?;
            let lane_mask = coerce_scalar(ctx, lane_mask, PtxScalarKind::U32)?;
            Ok(Some(PtxValue::Scalar(emit_cuda_shfl_xor_b32(
                ctx, value, lane_mask,
            ))))
        }
        "cuda_shfl_xor_f32" => {
            expect_cuda_arg_count(name, args, 2, span)?;
            let value = emit_expr(ctx, &args[0].value)?.into_scalar(span)?;
            let value = coerce_scalar(ctx, value, PtxScalarKind::F32)?;
            let lane_mask = emit_expr(ctx, &args[1].value)?.into_scalar(span)?;
            let lane_mask = coerce_scalar(ctx, lane_mask, PtxScalarKind::U32)?;
            Ok(Some(PtxValue::Scalar(emit_cuda_shfl_xor_f32(
                ctx, value, lane_mask,
            ))))
        }
        "cuda_warp_reduce_sum_u32" => {
            expect_cuda_arg_count(name, args, 1, span)?;
            let value = emit_expr(ctx, &args[0].value)?.into_scalar(span)?;
            let mut acc = coerce_scalar(ctx, value, PtxScalarKind::U32)?;
            for mask in [16, 8, 4, 2, 1] {
                let lane_mask = ctx.mov_u32_immediate(mask);
                let shuffled = emit_cuda_shfl_xor_b32(ctx, acc.clone(), lane_mask);
                let out = ctx.alloc_scalar(PtxScalarKind::U32);
                ctx.line(format!(
                    "add.u32 {}, {}, {};",
                    out.reg, acc.reg, shuffled.reg
                ));
                acc = out;
            }
            Ok(Some(PtxValue::Scalar(acc)))
        }
        "cuda_warp_reduce_sum_f32" => {
            expect_cuda_arg_count(name, args, 1, span)?;
            let value = emit_expr(ctx, &args[0].value)?.into_scalar(span)?;
            let mut acc = coerce_scalar(ctx, value, PtxScalarKind::F32)?;
            for mask in [16, 8, 4, 2, 1] {
                let lane_mask = ctx.mov_u32_immediate(mask);
                let shuffled = emit_cuda_shfl_xor_f32(ctx, acc.clone(), lane_mask);
                let out = ctx.alloc_scalar(PtxScalarKind::F32);
                ctx.line(format!(
                    "add.f32 {}, {}, {};",
                    out.reg, acc.reg, shuffled.reg
                ));
                acc = out;
            }
            Ok(Some(PtxValue::Scalar(acc)))
        }
        "cuda_cp_async_commit_group" => {
            expect_cuda_arg_count(name, args, 0, span)?;
            ctx.record_shared_op(ModuleSharedOpKind::CpAsyncCommitGroup);
            ctx.line("cp.async.commit_group;");
            Ok(Some(PtxValue::Void))
        }
        "cuda_cp_async_wait_group_0" => {
            expect_cuda_arg_count(name, args, 0, span)?;
            ctx.record_shared_op(ModuleSharedOpKind::CpAsyncWaitGroup);
            ctx.line("cp.async.wait_group 0;");
            Ok(Some(PtxValue::Void))
        }
        "cuda_require_tensor_cores" => {
            expect_cuda_arg_count(name, args, 0, span)?;
            ctx.record_tensor_op(ModuleTensorOpRequest::Fixed(ModuleTensorFixedKind::MmaSync));
            ctx.line("// kain.cuda: require tensor cores (mma.sync sm_75+)");
            Ok(Some(PtxValue::Void))
        }
        "cuda_require_wgmma" => {
            expect_cuda_arg_count(name, args, 0, span)?;
            ctx.record_tensor_op(ModuleTensorOpRequest::Fixed(
                ModuleTensorFixedKind::WgmmaFence,
            ));
            ctx.line("// kain.cuda: require warpgroup MMA (wgmma sm_90+)");
            Ok(Some(PtxValue::Void))
        }
        _ => Ok(None),
    }
}

fn expect_cuda_arg_count(
    name: &str,
    args: &[CallArg],
    expected: usize,
    span: Span,
) -> KainResult<()> {
    if args.len() == expected {
        return Ok(());
    }
    Err(KainError::codegen(
        format!(
            "CUDA intrinsic '{name}' expected {expected} argument(s), got {}",
            args.len()
        ),
        span,
    ))
}

fn emit_cuda_shfl_xor_b32(
    ctx: &mut PtxContext,
    value: ScalarValue,
    lane_mask: ScalarValue,
) -> ScalarValue {
    ctx.record_warp_op(ModuleWarpOpKind::ShflXorSync);
    let member_mask = ctx.mov_u32_immediate(u32::MAX);
    let out = ctx.alloc_scalar(PtxScalarKind::U32);
    ctx.line(format!(
        "shfl.sync.bfly.b32 {}, {}, {}, 31, {};",
        out.reg, value.reg, lane_mask.reg, member_mask.reg
    ));
    out
}

fn emit_cuda_shfl_xor_f32(
    ctx: &mut PtxContext,
    value: ScalarValue,
    lane_mask: ScalarValue,
) -> ScalarValue {
    let bits = ctx.alloc_scalar(PtxScalarKind::U32);
    ctx.line(format!("mov.b32 {}, {};", bits.reg, value.reg));
    let shuffled_bits = emit_cuda_shfl_xor_b32(ctx, bits, lane_mask);
    let out = ctx.alloc_scalar(PtxScalarKind::F32);
    ctx.line(format!("mov.b32 {}, {};", out.reg, shuffled_bits.reg));
    out
}

fn emit_min_max_call(
    ctx: &mut PtxContext,
    args: &[CallArg],
    span: Span,
    is_max: bool,
) -> KainResult<PtxValue> {
    let left = emit_expr(ctx, &args[0].value)?;
    let right = emit_expr(ctx, &args[1].value)?;
    match (left, right) {
        (PtxValue::Scalar(left), PtxValue::Scalar(right)) => Ok(PtxValue::Scalar(
            emit_min_max_scalar(ctx, left, right, span, is_max)?,
        )),
        (PtxValue::Vector(left), PtxValue::Vector(right)) if left.len() == right.len() => {
            let mut values = Vec::with_capacity(left.len());
            for (left_lane, right_lane) in left.into_iter().zip(right.into_iter()) {
                values.push(emit_min_max_scalar(
                    ctx, left_lane, right_lane, span, is_max,
                )?);
            }
            Ok(PtxValue::Vector(values))
        }
        _ => Err(KainError::codegen(
            "PTX min/max requires scalar-scalar or same-size vector-vector operands",
            span,
        )),
    }
}

fn emit_min_max_scalar(
    ctx: &mut PtxContext,
    left: ScalarValue,
    right: ScalarValue,
    span: Span,
    is_max: bool,
) -> KainResult<ScalarValue> {
    let kind = arithmetic_kind(left.kind, right.kind, span)?;
    let left = coerce_scalar(ctx, left, kind)?;
    let right = coerce_scalar(ctx, right, kind)?;
    let pred = ctx.alloc_scalar(PtxScalarKind::Pred);
    let out = ctx.alloc_scalar(kind);
    ctx.line(format!(
        "setp.lt.{} {}, {}, {};",
        kind.op_suffix(),
        pred.reg,
        left.reg,
        right.reg
    ));
    if is_max {
        ctx.line(format!(
            "selp.{} {}, {}, {}, {};",
            kind.op_suffix(),
            out.reg,
            right.reg,
            left.reg,
            pred.reg
        ));
    } else {
        ctx.line(format!(
            "selp.{} {}, {}, {}, {};",
            kind.op_suffix(),
            out.reg,
            left.reg,
            right.reg,
            pred.reg
        ));
    }
    Ok(out)
}

fn emit_inst(
    ctx: &mut PtxContext,
    mnemonic: &str,
    suffix: SuffixMode,
    result: &ScalarValue,
    operands: &[&ScalarValue],
) {
    let ty_suffix = match suffix {
        SuffixMode::Append | SuffixMode::Rounding => {
            format!(".{}", result.kind.op_suffix())
        }
        SuffixMode::Fixed | SuffixMode::None => String::new(),
        SuffixMode::Width => format!(".b{}", result.kind.width_bits()),
    };
    let operands = operands
        .iter()
        .map(|value| value.reg.as_str())
        .collect::<Vec<_>>()
        .join(", ");
    ctx.line(format!(
        "{mnemonic}{ty_suffix} {}, {};",
        result.reg, operands
    ));
}

fn emit_vector_constructor(
    ctx: &mut PtxContext,
    name: &str,
    args: &[CallArg],
    span: Span,
) -> KainResult<PtxValue> {
    let (lanes, kind) = vector_ctor_shape(name).ok_or_else(|| {
        KainError::codegen(format!("unsupported PTX vector constructor '{name}'"), span)
    })?;
    let mut values = Vec::with_capacity(lanes);
    for arg in args {
        match emit_expr(ctx, &arg.value)? {
            PtxValue::Scalar(value) => values.push(coerce_scalar(ctx, value, kind)?),
            PtxValue::Vector(vector) => {
                for value in vector {
                    values.push(coerce_scalar(ctx, value, kind)?);
                }
            }
            PtxValue::Void => {
                return Err(KainError::codegen(
                    "PTX vector constructors cannot consume void expressions",
                    span,
                ));
            }
        }
    }
    if values.len() != lanes {
        return Err(KainError::codegen(
            format!(
                "PTX vector constructor '{name}' expected {lanes} lane(s), got {}",
                values.len()
            ),
            span,
        ));
    }
    Ok(PtxValue::Vector(values))
}

fn emit_condition(ctx: &mut PtxContext, expr: &Expr) -> KainResult<ScalarValue> {
    let value = emit_expr(ctx, expr)?.into_scalar(expr.span())?;
    Ok(scalar_to_pred(ctx, value))
}

fn emit_builtin_vector_component(
    ctx: &mut PtxContext,
    vector: BuiltinVector,
    lane: usize,
) -> ScalarValue {
    let axis = match lane {
        0 => "x",
        1 => "y",
        _ => "z",
    };
    match vector {
        BuiltinVector::GroupThreadId => {
            let out = ctx.alloc_scalar(PtxScalarKind::U32);
            ctx.line(format!("mov.u32 {}, %tid.{axis};", out.reg));
            out
        }
        BuiltinVector::GroupId => {
            let out = ctx.alloc_scalar(PtxScalarKind::U32);
            ctx.line(format!("mov.u32 {}, %ctaid.{axis};", out.reg));
            out
        }
        BuiltinVector::BlockDim => {
            let out = ctx.alloc_scalar(PtxScalarKind::U32);
            ctx.line(format!("mov.u32 {}, %ntid.{axis};", out.reg));
            out
        }
        BuiltinVector::GridDim => {
            let out = ctx.alloc_scalar(PtxScalarKind::U32);
            ctx.line(format!("mov.u32 {}, %nctaid.{axis};", out.reg));
            out
        }
        BuiltinVector::DispatchThreadId => {
            let ctaid = ctx.alloc_scalar(PtxScalarKind::U32);
            let ntid = ctx.alloc_scalar(PtxScalarKind::U32);
            let tid = ctx.alloc_scalar(PtxScalarKind::U32);
            let out = ctx.alloc_scalar(PtxScalarKind::U32);
            ctx.line(format!("mov.u32 {}, %ctaid.{axis};", ctaid.reg));
            ctx.line(format!("mov.u32 {}, %ntid.{axis};", ntid.reg));
            ctx.line(format!("mov.u32 {}, %tid.{axis};", tid.reg));
            ctx.line(format!(
                "mad.lo.u32 {}, {}, {}, {};",
                out.reg, ctaid.reg, ntid.reg, tid.reg
            ));
            out
        }
    }
}

fn emit_group_index(ctx: &mut PtxContext) -> ScalarValue {
    let tx = emit_builtin_vector_component(ctx, BuiltinVector::GroupThreadId, 0);
    let ty = emit_builtin_vector_component(ctx, BuiltinVector::GroupThreadId, 1);
    let tz = emit_builtin_vector_component(ctx, BuiltinVector::GroupThreadId, 2);
    let y_offset = ctx.alloc_scalar(PtxScalarKind::U32);
    let xy = ctx.alloc_scalar(PtxScalarKind::U32);
    let z_stride = ctx.alloc_scalar(PtxScalarKind::U32);
    let out = ctx.alloc_scalar(PtxScalarKind::U32);

    ctx.line(format!(
        "mul.lo.u32 {}, {}, {};",
        y_offset.reg, ty.reg, ctx.local_size[0]
    ));
    ctx.line(format!("add.u32 {}, {}, {};", xy.reg, tx.reg, y_offset.reg));
    ctx.line(format!(
        "mul.lo.u32 {}, {}, {};",
        z_stride.reg, ctx.local_size[0], ctx.local_size[1]
    ));
    ctx.line(format!(
        "mad.lo.u32 {}, {}, {}, {};",
        out.reg, tz.reg, z_stride.reg, xy.reg
    ));
    out
}

fn emit_storage_address(
    ctx: &mut PtxContext,
    base: &ScalarValue,
    index: &ScalarValue,
    stride: u32,
    component_offset: u32,
) -> ScalarValue {
    let offset = ctx.alloc_scalar(PtxScalarKind::U64);
    let address = ctx.alloc_scalar(PtxScalarKind::U64);
    ctx.line(format!(
        "mul.wide.u32 {}, {}, {};",
        offset.reg, index.reg, stride
    ));
    ctx.line(format!(
        "add.u64 {}, {}, {};",
        address.reg, base.reg, offset.reg
    ));
    if component_offset == 0 {
        address
    } else {
        let adjusted = ctx.alloc_scalar(PtxScalarKind::U64);
        ctx.line(format!(
            "add.u64 {}, {}, {};",
            adjusted.reg, address.reg, component_offset
        ));
        adjusted
    }
}

fn scalar_to_pred(ctx: &mut PtxContext, value: ScalarValue) -> ScalarValue {
    if value.kind == PtxScalarKind::Pred {
        return value;
    }
    let pred = ctx.alloc_scalar(PtxScalarKind::Pred);
    let zero = if value.kind.is_float() {
        if value.kind.width_bits() == 64 {
            "0d0000000000000000"
        } else {
            "0f00000000"
        }
    } else {
        "0"
    };
    ctx.line(format!(
        "setp.ne.{} {}, {}, {};",
        value.kind.op_suffix(),
        pred.reg,
        value.reg,
        zero
    ));
    pred
}

fn coerce_scalar(
    ctx: &mut PtxContext,
    value: ScalarValue,
    target: PtxScalarKind,
) -> KainResult<ScalarValue> {
    if value.kind == target {
        return Ok(value);
    }
    if value.kind == PtxScalarKind::Pred && target == PtxScalarKind::U32 {
        let out = ctx.alloc_scalar(PtxScalarKind::U32);
        ctx.line(format!("selp.u32 {}, 1, 0, {};", out.reg, value.reg));
        return Ok(out);
    }
    if target == PtxScalarKind::Pred {
        return Ok(scalar_to_pred(ctx, value));
    }

    if let Some(instr) = cvt_inst(value.kind, target) {
        let out = ctx.alloc_scalar(target);
        ctx.line(format!("{instr} {}, {};", out.reg, value.reg));
        return Ok(out);
    }

    if value.kind.width_bits() == target.width_bits() {
        let out = ctx.alloc_scalar(target);
        ctx.line(format!(
            "mov.b{} {}, {};",
            target.width_bits(),
            out.reg,
            value.reg
        ));
        return Ok(out);
    }

    Err(KainError::codegen(
        format!("PTX backend cannot coerce {:?} to {:?}", value.kind, target),
        Span::default(),
    ))
}

fn arithmetic_kind(
    left: PtxScalarKind,
    right: PtxScalarKind,
    span: Span,
) -> KainResult<PtxScalarKind> {
    if left.is_float() || right.is_float() {
        return Ok(PtxScalarKind::F32);
    }
    if left == PtxScalarKind::U64 || right == PtxScalarKind::U64 {
        return Ok(PtxScalarKind::U64);
    }
    if left == PtxScalarKind::S32 || right == PtxScalarKind::S32 {
        return Ok(PtxScalarKind::S32);
    }
    if left == PtxScalarKind::U32 && right == PtxScalarKind::U32 {
        return Ok(PtxScalarKind::U32);
    }
    Err(KainError::codegen(
        "PTX backend cannot infer arithmetic kind for this operation",
        span,
    ))
}

fn zero_value_for_type(ctx: &mut PtxContext, ty: &Type, span: Span) -> KainResult<PtxValue> {
    let Some(shape) = type_shape(ty) else {
        return Err(KainError::codegen(
            format!(
                "PTX backend cannot synthesize zero value for {}",
                type_name(ty)
            ),
            span,
        ));
    };
    if shape.lanes == 1 {
        Ok(PtxValue::Scalar(zero_scalar(ctx, shape.scalar)))
    } else {
        let mut lanes = Vec::with_capacity(shape.lanes);
        for _ in 0..shape.lanes {
            lanes.push(zero_scalar(ctx, shape.scalar));
        }
        Ok(PtxValue::Vector(lanes))
    }
}

fn zero_scalar(ctx: &mut PtxContext, kind: PtxScalarKind) -> ScalarValue {
    let out = ctx.alloc_scalar(kind);
    match kind {
        PtxScalarKind::F32 => ctx.line(format!("mov.f32 {}, 0f00000000;", out.reg)),
        PtxScalarKind::S32 => ctx.line(format!("mov.s32 {}, 0;", out.reg)),
        PtxScalarKind::U32 => ctx.line(format!("mov.u32 {}, 0;", out.reg)),
        PtxScalarKind::U64 => ctx.line(format!("mov.u64 {}, 0;", out.reg)),
        PtxScalarKind::Pred => ctx.line(format!("setp.ne.u32 {}, 0, 0;", out.reg)),
    }
    out
}

fn module_scalar_kind(kind: PtxScalarKind) -> ModuleScalarKind {
    match kind {
        PtxScalarKind::U32 => ModuleScalarKind::U32,
        PtxScalarKind::S32 => ModuleScalarKind::S32,
        PtxScalarKind::F32 => ModuleScalarKind::F32,
        PtxScalarKind::U64 => ModuleScalarKind::U64,
        PtxScalarKind::Pred => ModuleScalarKind::Pred,
    }
}

fn emitted_kind_for_module_param(param: &ModuleKernelParam) -> Option<PtxScalarKind> {
    match param.encoding {
        ModuleKernelParamEncoding::Scalar(kind) => match kind {
            ModuleScalarKind::Pred => Some(PtxScalarKind::Pred),
            ModuleScalarKind::U32 => Some(PtxScalarKind::U32),
            ModuleScalarKind::S32 => Some(PtxScalarKind::S32),
            ModuleScalarKind::U64 => Some(PtxScalarKind::U64),
            ModuleScalarKind::F32 => Some(PtxScalarKind::F32),
            ModuleScalarKind::S64
            | ModuleScalarKind::F16
            | ModuleScalarKind::BF16
            | ModuleScalarKind::F64 => None,
        },
        ModuleKernelParamEncoding::Pointer { .. } | ModuleKernelParamEncoding::Descriptor64 => {
            Some(PtxScalarKind::U64)
        }
    }
}

fn module_param_label(param: &ModuleKernelParam) -> &'static str {
    match param.encoding {
        ModuleKernelParamEncoding::Scalar(kind) => match kind {
            ModuleScalarKind::Pred => "pred",
            ModuleScalarKind::U32 => "u32",
            ModuleScalarKind::S32 => "s32",
            ModuleScalarKind::U64 => "u64",
            ModuleScalarKind::S64 => "s64",
            ModuleScalarKind::F16 => "f16",
            ModuleScalarKind::BF16 => "bf16",
            ModuleScalarKind::F32 => "f32",
            ModuleScalarKind::F64 => "f64",
        },
        ModuleKernelParamEncoding::Pointer { .. } => "ptr.u64",
        ModuleKernelParamEncoding::Descriptor64 => "descriptor64",
    }
}

#[derive(Clone, Copy)]
struct TypeShape {
    scalar: PtxScalarKind,
    lanes: usize,
    stride: u32,
    component_stride: u32,
    load_suffix: &'static str,
    store_suffix: &'static str,
}

fn type_shape(ty: &Type) -> Option<TypeShape> {
    match ty {
        Type::Named { name, .. } => match name.as_str() {
            "Float" | "f32" | "F32" => scalar_shape_for(name, PtxScalarKind::F32, "f32", "f32"),
            "Int" | "i32" | "I32" => scalar_shape_for(name, PtxScalarKind::S32, "s32", "s32"),
            "UInt" | "u32" | "U32" | "Bool" => {
                scalar_shape_for(name, PtxScalarKind::U32, "u32", "u32")
            }
            "UInt64" | "u64" | "U64" => scalar_shape_for(name, PtxScalarKind::U64, "u64", "u64"),
            "i8" | "I8" => scalar_shape_for(name, PtxScalarKind::S32, "s8", "u8"),
            "u8" | "U8" => scalar_shape_for(name, PtxScalarKind::U32, "u8", "u8"),
            "i16" | "I16" => scalar_shape_for(name, PtxScalarKind::S32, "s16", "u16"),
            "u16" | "U16" => scalar_shape_for(name, PtxScalarKind::U32, "u16", "u16"),
            "f16" | "F16" | "bf16" | "BF16" => {
                scalar_shape_for(name, PtxScalarKind::U32, "u16", "u16")
            }
            "Vec2" => vector_shape(name, PtxScalarKind::F32, 2),
            "Vec3" => vector_shape(name, PtxScalarKind::F32, 3),
            "Vec4" => vector_shape(name, PtxScalarKind::F32, 4),
            "IVec2" => vector_shape(name, PtxScalarKind::S32, 2),
            "IVec3" => vector_shape(name, PtxScalarKind::S32, 3),
            "IVec4" => vector_shape(name, PtxScalarKind::S32, 4),
            "UVec2" => vector_shape(name, PtxScalarKind::U32, 2),
            "UVec3" => vector_shape(name, PtxScalarKind::U32, 3),
            "UVec4" => vector_shape(name, PtxScalarKind::U32, 4),
            _ => None,
        },
        Type::Unit(_) => Some(TypeShape {
            scalar: PtxScalarKind::U32,
            lanes: 0,
            stride: 0,
            component_stride: 0,
            load_suffix: "u32",
            store_suffix: "u32",
        }),
        _ => None,
    }
}

fn scalar_shape_for(
    element_type: &str,
    scalar: PtxScalarKind,
    load_suffix: &'static str,
    store_suffix: &'static str,
) -> Option<TypeShape> {
    scalar_shape(
        scalar,
        gpu_storage_element_stride_bytes(element_type)? as u32,
        load_suffix,
        store_suffix,
    )
}

fn scalar_shape(
    scalar: PtxScalarKind,
    bytes: u32,
    load_suffix: &'static str,
    store_suffix: &'static str,
) -> Option<TypeShape> {
    Some(TypeShape {
        scalar,
        lanes: 1,
        stride: bytes,
        component_stride: bytes,
        load_suffix,
        store_suffix,
    })
}

fn vector_shape(element_type: &str, scalar: PtxScalarKind, lanes: usize) -> Option<TypeShape> {
    Some(TypeShape {
        scalar,
        lanes,
        stride: gpu_storage_element_stride_bytes(element_type)? as u32,
        component_stride: 4,
        load_suffix: scalar.op_suffix(),
        store_suffix: scalar.op_suffix(),
    })
}

fn scalar_kind_for_type(ty: &Type) -> Option<PtxScalarKind> {
    type_shape(ty).and_then(|shape| (shape.lanes == 1).then_some(shape.scalar))
}

fn vector_ctor_shape(name: &str) -> Option<(usize, PtxScalarKind)> {
    let lower = name.to_ascii_lowercase();
    let lanes = if lower.ends_with('2') {
        2
    } else if lower.ends_with('3') {
        3
    } else if lower.ends_with('4') {
        4
    } else {
        return None;
    };
    let kind = if lower.starts_with("ivec") {
        PtxScalarKind::S32
    } else if lower.starts_with("uvec") {
        PtxScalarKind::U32
    } else {
        PtxScalarKind::F32
    };
    Some((lanes, kind))
}

fn vector_lane(field: &str) -> Option<usize> {
    match field {
        "x" | "r" => Some(0),
        "y" | "g" => Some(1),
        "z" | "b" => Some(2),
        "w" | "a" => Some(3),
        _ => None,
    }
}

fn lane_name(lane: usize) -> &'static str {
    match lane {
        0 => "x",
        1 => "y",
        2 => "z",
        _ => "w",
    }
}

fn validate_supported_shader(shader: &TypedShader) -> KainResult<()> {
    let mut runtime_bindings = HashSet::new();
    for uniform in &shader.ast.uniforms {
        if is_local_size_param(&uniform.name) {
            continue;
        }
        if !runtime_bindings.insert(uniform.binding) {
            return Err(KainError::codegen(
                format!(
                    "PTX backend requires unique runtime binding slots; '{}' reuses @{}",
                    uniform.name, uniform.binding
                ),
                uniform.span,
            ));
        }
        if is_storage_buffer(&uniform.ty) {
            let elem = storage_buffer_elem_type(&uniform.ty, uniform.span);
            if type_shape(&elem).is_none() {
                return Err(KainError::codegen(
                    format!(
                        "PTX backend does not support StorageBuffer<{}> for '{}'",
                        type_name(&elem),
                        uniform.name
                    ),
                    uniform.span,
                ));
            }
            continue;
        }
        if type_shape(&uniform.ty).is_some() {
            continue;
        }
        return Err(KainError::codegen(
            format!(
                "PTX backend v1 rejects non-buffer/non-scalar uniform '{}' of type {}",
                uniform.name,
                type_name(&uniform.ty)
            ),
            uniform.span,
        ));
    }
    Ok(())
}

fn is_storage_buffer(ty: &Type) -> bool {
    matches!(ty, Type::Named { name, .. } if name == "StorageBuffer")
}

fn storage_buffer_elem_type(buffer_ty: &Type, span: Span) -> Type {
    match buffer_ty {
        Type::Named { name, generics, .. } if name == "StorageBuffer" => {
            generics.first().cloned().unwrap_or(Type::Named {
                name: "Float".into(),
                generics: vec![],
                span,
            })
        }
        _ => Type::Named {
            name: "Float".into(),
            generics: vec![],
            span,
        },
    }
}

fn is_local_size_param(name: &str) -> bool {
    matches!(name, "LOCAL_SIZE_X" | "LOCAL_SIZE_Y" | "LOCAL_SIZE_Z")
}

fn is_uint_type(ty: &Type) -> bool {
    matches!(
        ty,
        Type::Named { name, .. }
            if name == "UInt"
                || name == "u32"
                || name == "UInt64"
                || name == "u64"
    )
}

fn type_name(ty: &Type) -> String {
    match ty {
        Type::Named { name, generics, .. } if generics.is_empty() => name.clone(),
        Type::Named { name, generics, .. } => format!(
            "{}<{}>",
            name,
            generics
                .iter()
                .map(type_name)
                .collect::<Vec<_>>()
                .join(", ")
        ),
        Type::Unit(_) => "Void".to_string(),
        Type::Array(inner, len, _) => format!("[{}; {len}]", type_name(inner)),
        Type::Slice(inner, _) => format!("[{}]", type_name(inner)),
        _ => "<unsupported>".to_string(),
    }
}

fn sanitize_ident(name: &str) -> String {
    let mut out = String::new();
    for (index, ch) in name.chars().enumerate() {
        let valid = ch.is_ascii_alphanumeric() || ch == '_';
        if index == 0 && ch.is_ascii_digit() {
            out.push('_');
        }
        out.push(if valid { ch } else { '_' });
    }
    if out.is_empty() {
        "_kain".to_string()
    } else {
        out
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use kain_core::ast::{Block, Shader, Uniform};
    use kain_core::types::TypedShader;

    fn named(name: &str) -> Type {
        Type::Named {
            name: name.to_string(),
            generics: vec![],
            span: Span::default(),
        }
    }

    fn storage(elem: Type) -> Type {
        Type::Named {
            name: "StorageBuffer".to_string(),
            generics: vec![elem],
            span: Span::default(),
        }
    }

    fn test_shader(stage: ShaderStage, uniforms: Vec<Uniform>) -> TypedShader {
        TypedShader {
            ast: Shader {
                name: "test_kernel".to_string(),
                stage,
                inputs: Vec::new(),
                outputs: named("Vec4"),
                workgroup_size: None,
                uniforms,
                body: Block {
                    stmts: Vec::new(),
                    span: Span::default(),
                },
                span: Span::default(),
            },
            input_types: Vec::new(),
            output_type: kain_core::types::ResolvedType::Unknown,
        }
    }

    fn storage_uniform(name: &str, binding: u32) -> Uniform {
        Uniform {
            name: name.to_string(),
            ty: storage(named("Float")),
            binding,
            span: Span::default(),
        }
    }

    #[test]
    fn ptx_vec3_storage_stride_matches_std430_style_alignment() {
        let shape = type_shape(&named("Vec3")).expect("vec3 shape");
        assert_eq!(shape.stride, 16);
        assert_eq!(shape.lanes, 3);
    }

    #[test]
    fn ptx_storage_shapes_stay_in_lockstep_with_shared_gpu_stride_helper() {
        for name in [
            "Bool", "Float", "Int", "UInt", "u8", "i16", "f16", "Vec2", "Vec3", "Vec4", "IVec3",
            "UVec3",
        ] {
            assert_eq!(
                type_shape(&named(name)).expect("ptx type shape").stride,
                gpu_storage_element_stride_bytes(name).expect("shared stride helper") as u32
            );
        }
    }

    #[test]
    fn ptx_rejects_non_compute_shader() {
        let shader = test_shader(ShaderStage::Fragment, vec![storage_uniform("src", 0)]);
        let err = emit_shader(&shader, DEFAULT_PTX_ARCH).expect_err("fragment shader must fail");
        assert!(err.to_string().contains("only supports compute shaders"));
    }

    #[test]
    fn ptx_rejects_duplicate_runtime_bindings() {
        let shader = test_shader(
            ShaderStage::Compute,
            vec![storage_uniform("src", 0), storage_uniform("dst", 0)],
        );
        let err = emit_shader(&shader, DEFAULT_PTX_ARCH).expect_err("duplicate binding must fail");
        assert!(err.to_string().contains("unique runtime binding slots"));
    }

    #[test]
    fn auto_target_arch_follows_required_kernel_floor() {
        let options = PtxCodegenOptions::auto();
        assert_eq!(
            options.resolve_module_target_arch(ModuleArch::Sm75),
            ModuleArch::Sm75
        );
        assert_eq!(
            options.resolve_module_target_arch(ModuleArch::Sm50),
            ModuleArch::Sm50
        );
    }
}
