//! SPIR-V Code Generation for GPU shaders

use kain_core::ast::{
    BinaryOp, Block, ElseBranch, Expr, Pattern, ShaderStage, Stmt, Type, UnaryOp,
};
use kain_core::error::{KainError, KainResult};
use kain_core::types::{TypedItem, TypedProgram, TypedShader};
use rspirv::binary::Assemble;
use rspirv::dr::{Builder, Operand};
use rspirv::spirv::{
    AddressingModel, Capability, Decoration, ExecutionMode, ExecutionModel, MemoryModel,
    StorageClass,
};
use std::collections::{HashMap, HashSet};

pub fn generate(program: &TypedProgram) -> KainResult<Vec<u8>> {
    let mut builder = Builder::new();

    // Set capabilities and memory model
    builder.capability(Capability::Shader);
    // Add VulkanMemoryModel if targeting Vulkan, but GLSL450 is standard for now
    builder.memory_model(AddressingModel::Logical, MemoryModel::GLSL450);

    for item in &program.items {
        if let TypedItem::Shader(shader) = item {
            emit_shader(&mut builder, shader)?;
        }
    }

    let module = builder.module();
    let bytes: Vec<u8> = module
        .assemble()
        .iter()
        .flat_map(|w| w.to_le_bytes())
        .collect();
    Ok(bytes)
}

struct ShaderContext<'a> {
    b: &'a mut Builder,
    // Name -> (SPIR-V ID, AST Type, IsPointer)
    vars: HashMap<String, VarBinding>,
    output_var: Option<u32>,
    // Track which variables are struct-wrapped uniforms (need AccessChain)
    struct_uniforms: HashSet<String>,
    // Track storage-buffer uniforms to emit runtime array indexing loads/stores.
    storage_buffers: HashSet<String>,
    // Cache GLSL extension import
    glsl_ext: Option<u32>,
    loop_continue_targets: Vec<u32>,
    loop_break_targets: Vec<u32>,
    // Pre-hoisted local variable slots — all OpVariable must be in the first block.
    // Maps binding name -> pre-allocated slots in source discovery order.
    hoisted_vars: HashMap<String, Vec<(u32, u32)>>,
}

#[derive(Clone)]
struct VarBinding {
    id: u32,
    ty: Type,
    is_ptr: bool,
}

fn emit_shader(b: &mut Builder, shader: &TypedShader) -> KainResult<()> {
    let exec_model = match shader.ast.stage {
        ShaderStage::Vertex => ExecutionModel::Vertex,
        ShaderStage::Fragment => ExecutionModel::Fragment,
        ShaderStage::Compute => ExecutionModel::GLCompute,
        ShaderStage::Surface => ExecutionModel::Fragment, // Surface shaders compile to fragment
    };

    // 1. Define Basic Types
    let void = b.type_void();

    // 2. Define Entry Point Function Type
    let fn_void_void = b.type_function(void, vec![]);

    // 3. Declare Variables (Global Interface)
    let mut interface_vars = vec![];
    let mut ctx_vars = HashMap::new();
    let mut struct_uniforms = HashSet::new();
    let mut storage_buffers = HashSet::new();
    let mut storage_buffer_type_cache: HashMap<String, u32> = HashMap::new();
    let mut uniform_wrapper_type_cache: HashMap<String, u32> = HashMap::new();
    let mut compute_input_params: Vec<(String, Type)> = Vec::new();
    let mut local_size_values: [u32; 3] = [8, 8, 1];

    // Inputs
    for (i, param) in shader.ast.inputs.iter().enumerate() {
        if exec_model == ExecutionModel::GLCompute {
            compute_input_params.push((param.name.clone(), param.ty.clone()));
            continue;
        }
        let ty = map_ast_type(b, &param.ty);
        let ptr_ty = b.type_pointer(None, StorageClass::Input, ty);
        let var = b.variable(ptr_ty, None, StorageClass::Input, None);
        b.decorate(
            var,
            Decoration::Location,
            vec![Operand::LiteralBit32(i as u32)],
        );
        interface_vars.push(var);
        ctx_vars.insert(
            param.name.clone(),
            VarBinding {
                id: var,
                ty: param.ty.clone(),
                is_ptr: true,
            },
        );
    }

    // Outputs
    let output_var = if !is_void(&shader.ast.outputs) {
        let output_ty = map_ast_type(b, &shader.ast.outputs);
        let ptr_ty = b.type_pointer(None, StorageClass::Output, output_ty);
        let var = b.variable(ptr_ty, None, StorageClass::Output, None);

        // Vertex shader output is @builtin(position) for Vec4, otherwise use Location
        if exec_model == ExecutionModel::Vertex && is_vec4(&shader.ast.outputs) {
            b.decorate(
                var,
                Decoration::BuiltIn,
                vec![Operand::BuiltIn(rspirv::spirv::BuiltIn::Position)],
            );
        } else {
            b.decorate(var, Decoration::Location, vec![Operand::LiteralBit32(0)]);
        }

        interface_vars.push(var);
        Some(var)
    } else {
        None
    };

    // Uniforms
    for uniform in &shader.ast.uniforms {
        if matches!(shader.ast.stage, ShaderStage::Compute) && is_local_size_param(&uniform.name) {
            let slot = match uniform.name.as_str() {
                "LOCAL_SIZE_X" => 0usize,
                "LOCAL_SIZE_Y" => 1usize,
                "LOCAL_SIZE_Z" => 2usize,
                _ => 0usize,
            };
            let default_value = match slot {
                0 | 1 => 8,
                _ => 1,
            };
            local_size_values[slot] = default_value;
            let uint_ty = b.type_int(32, 0);
            let const_id = b.constant_bit32(uint_ty, default_value);
            ctx_vars.insert(
                uniform.name.clone(),
                VarBinding {
                    id: const_id,
                    ty: Type::Named {
                        name: "UInt".into(),
                        generics: vec![],
                        span: uniform.span,
                    },
                    is_ptr: false,
                },
            );
            continue;
        }
        if is_permutation_param(&uniform.name) {
            if let Some(spec_id) = emit_permutation_spec_constant(b, &uniform.ty, uniform.binding) {
                ctx_vars.insert(
                    uniform.name.clone(),
                    VarBinding {
                        id: spec_id,
                        ty: uniform.ty.clone(),
                        is_ptr: false,
                    },
                );
                continue;
            }
        }

        let is_sampler = matches!(&uniform.ty, Type::Named { name, .. } if name == "Sampler2D");

        if is_sampler {
            let inner_ty = map_ast_type(b, &uniform.ty);
            let ptr_ty = b.type_pointer(None, StorageClass::UniformConstant, inner_ty);
            let var = b.variable(ptr_ty, None, StorageClass::UniformConstant, None);
            b.decorate(
                var,
                Decoration::DescriptorSet,
                vec![Operand::LiteralBit32(0)],
            );
            b.decorate(
                var,
                Decoration::Binding,
                vec![Operand::LiteralBit32(uniform.binding)],
            );
            interface_vars.push(var);
            ctx_vars.insert(
                uniform.name.clone(),
                VarBinding {
                    id: var,
                    ty: uniform.ty.clone(),
                    is_ptr: true,
                },
            );
        } else {
            let is_storage_buffer = is_storage_buffer(&uniform.ty);
            let (storage_class, pointee_ty) = if is_storage_buffer {
                // StorageBuffer<T> is already lowered to a decorated block-wrapped runtime array.
                (
                    StorageClass::StorageBuffer,
                    get_or_create_storage_buffer_type(
                        b,
                        &uniform.ty,
                        &mut storage_buffer_type_cache,
                    ),
                )
            } else {
                (
                    StorageClass::Uniform,
                    get_or_create_uniform_wrapper_type(
                        b,
                        &uniform.ty,
                        &mut uniform_wrapper_type_cache,
                    ),
                )
            };
            let ptr_ty = b.type_pointer(None, storage_class, pointee_ty);
            let var = b.variable(ptr_ty, None, storage_class, None);
            b.decorate(
                var,
                Decoration::DescriptorSet,
                vec![Operand::LiteralBit32(0)],
            );
            b.decorate(
                var,
                Decoration::Binding,
                vec![Operand::LiteralBit32(uniform.binding)],
            );
            interface_vars.push(var);
            ctx_vars.insert(
                uniform.name.clone(),
                VarBinding {
                    id: var,
                    ty: uniform.ty.clone(),
                    is_ptr: true,
                },
            );
            if is_storage_buffer {
                storage_buffers.insert(uniform.name.clone());
            } else {
                struct_uniforms.insert(uniform.name.clone());
            }
        }
    }

    if exec_model == ExecutionModel::GLCompute {
        let uint = b.type_int(32, 0);
        let uvec3 = b.type_vector(uint, 3);
        let uvec3_ptr = b.type_pointer(None, StorageClass::Input, uvec3);
        let uint_ptr = b.type_pointer(None, StorageClass::Input, uint);

        let gid = b.variable(uvec3_ptr, None, StorageClass::Input, None);
        b.decorate(
            gid,
            Decoration::BuiltIn,
            vec![Operand::BuiltIn(rspirv::spirv::BuiltIn::GlobalInvocationId)],
        );
        interface_vars.push(gid);

        let lid = b.variable(uvec3_ptr, None, StorageClass::Input, None);
        b.decorate(
            lid,
            Decoration::BuiltIn,
            vec![Operand::BuiltIn(rspirv::spirv::BuiltIn::LocalInvocationId)],
        );
        interface_vars.push(lid);

        let wid = b.variable(uvec3_ptr, None, StorageClass::Input, None);
        b.decorate(
            wid,
            Decoration::BuiltIn,
            vec![Operand::BuiltIn(rspirv::spirv::BuiltIn::WorkgroupId)],
        );
        interface_vars.push(wid);

        let lindex = b.variable(uint_ptr, None, StorageClass::Input, None);
        b.decorate(
            lindex,
            Decoration::BuiltIn,
            vec![Operand::BuiltIn(
                rspirv::spirv::BuiltIn::LocalInvocationIndex,
            )],
        );
        interface_vars.push(lindex);

        let uvec3_ty = Type::Named {
            name: "UVec3".into(),
            generics: vec![],
            span: shader.ast.span,
        };
        let uint_ty = Type::Named {
            name: "UInt".into(),
            generics: vec![],
            span: shader.ast.span,
        };
        ctx_vars.insert(
            "global_invocation_id".into(),
            VarBinding {
                id: gid,
                ty: uvec3_ty.clone(),
                is_ptr: true,
            },
        );
        ctx_vars.insert(
            "local_invocation_id".into(),
            VarBinding {
                id: lid,
                ty: uvec3_ty.clone(),
                is_ptr: true,
            },
        );
        ctx_vars.insert(
            "workgroup_id".into(),
            VarBinding {
                id: wid,
                ty: uvec3_ty,
                is_ptr: true,
            },
        );
        ctx_vars.insert(
            "local_invocation_index".into(),
            VarBinding {
                id: lindex,
                ty: uint_ty.clone(),
                is_ptr: true,
            },
        );
        // Friendly aliases matching the HLSL backend naming.
        ctx_vars.insert(
            "dispatch_thread_id".into(),
            VarBinding {
                id: gid,
                ty: Type::Named {
                    name: "UVec3".into(),
                    generics: vec![],
                    span: shader.ast.span,
                },
                is_ptr: true,
            },
        );
        ctx_vars.insert(
            "group_thread_id".into(),
            VarBinding {
                id: lid,
                ty: Type::Named {
                    name: "UVec3".into(),
                    generics: vec![],
                    span: shader.ast.span,
                },
                is_ptr: true,
            },
        );
        ctx_vars.insert(
            "group_id".into(),
            VarBinding {
                id: wid,
                ty: Type::Named {
                    name: "UVec3".into(),
                    generics: vec![],
                    span: shader.ast.span,
                },
                is_ptr: true,
            },
        );
        ctx_vars.insert(
            "group_index".into(),
            VarBinding {
                id: lindex,
                ty: uint_ty,
                is_ptr: true,
            },
        );

        // Compute shader user params are aliases to built-ins, not Location inputs.
        // This supports patterns like `shader compute foo(id: UVec3)`.
        for (name, ty) in compute_input_params.iter() {
            let lowered = if matches!(ty, Type::Named { name, .. } if name == "UInt" || name == "u32")
            {
                VarBinding {
                    id: lindex,
                    ty: ty.clone(),
                    is_ptr: true,
                }
            } else {
                VarBinding {
                    id: gid,
                    ty: ty.clone(),
                    is_ptr: true,
                }
            };
            ctx_vars.insert(name.clone(), lowered);
        }
    }

    // SPIR-V §2.16: All OpVariable must be the first instructions of the first block.
    // Seed known-types with shader input parameters (e.g. id: UVec3), then
    // walk the body to collect all let-binding types for pre-hoisting.
    let mut binding_types: Vec<(String, Type)> = Vec::new();
    let mut seed_known: HashMap<String, Type> = shader
        .ast
        .inputs
        .iter()
        .map(|p| (p.name.clone(), p.ty.clone()))
        .collect();
    seed_known.extend(
        shader
            .ast
            .uniforms
            .iter()
            .map(|u| (u.name.clone(), u.ty.clone())),
    );
    if exec_model == ExecutionModel::GLCompute {
        let uvec3_ty = Type::Named {
            name: "UVec3".into(),
            generics: vec![],
            span: shader.ast.span,
        };
        let uint_ty = Type::Named {
            name: "UInt".into(),
            generics: vec![],
            span: shader.ast.span,
        };
        seed_known.insert("global_invocation_id".into(), uvec3_ty.clone());
        seed_known.insert("local_invocation_id".into(), uvec3_ty.clone());
        seed_known.insert("workgroup_id".into(), uvec3_ty.clone());
        seed_known.insert("dispatch_thread_id".into(), uvec3_ty.clone());
        seed_known.insert("group_thread_id".into(), uvec3_ty.clone());
        seed_known.insert("group_id".into(), uvec3_ty);
        seed_known.insert("local_invocation_index".into(), uint_ty.clone());
        seed_known.insert("group_index".into(), uint_ty);
    }
    collect_binding_types_seeded(&shader.ast.body, seed_known, &mut binding_types);

    // Emit all Function-pointer types and OpVariable allocations in the global section.
    let mut hoisted_vars: HashMap<String, Vec<(u32, u32)>> = HashMap::new();
    for (name, kain_ty) in &binding_types {
        let type_id = map_ast_type(b, kain_ty);
        let ptr_ty = b.type_pointer(None, StorageClass::Function, type_id);
        // OpVariable with Function storage class must be inside a function —
        // we emit it right after begin_function/begin_block below. Store ptr_ty for now.
        hoisted_vars
            .entry(name.clone())
            .or_default()
            .push((0, ptr_ty)); // 0 = placeholder, replaced below
    }

    // 4. Function Body
    let main_fn = b
        .begin_function(
            void,
            None,
            rspirv::spirv::FunctionControl::NONE,
            fn_void_void,
        )
        .unwrap();
    b.begin_block(None).unwrap();

    // Now emit the actual OpVariable instructions at the very top of the first block.
    for (_name, slots) in hoisted_vars.iter_mut() {
        for slot in slots.iter_mut() {
            let var_id = b.variable(slot.1, None, StorageClass::Function, None);
            slot.0 = var_id;
        }
    }
    // Repackage: rename from (var_placeholder, ptr_ty) to (var_id, ptr_ty)
    // The map already has var_id in slot 0 now, ptr_ty in slot 1.

    let mut ctx = ShaderContext {
        b,
        vars: ctx_vars,
        output_var,
        struct_uniforms,
        storage_buffers,
        glsl_ext: None,
        loop_continue_targets: vec![],
        loop_break_targets: vec![],
        hoisted_vars,
    };

    emit_block(&mut ctx, &shader.ast.body)?;

    // Ensure we always have a return
    if shader
        .ast
        .body
        .stmts
        .last()
        .map_or(true, |s| !matches!(s, Stmt::Return(_, _)))
    {
        ctx.b.ret().unwrap();
    }

    ctx.b.end_function().unwrap();

    // 5. Entry Point
    b.entry_point(exec_model, main_fn, &shader.ast.name, interface_vars);

    if exec_model == ExecutionModel::Fragment {
        b.execution_mode(main_fn, ExecutionMode::OriginUpperLeft, vec![]);
    } else if exec_model == ExecutionModel::GLCompute {
        // Emit fixed local size for broad runtime compatibility.
        // Some wgpu backends reject ExecutionModeId/LocalSizeId in SPIR-V modules.
        b.execution_mode(
            main_fn,
            ExecutionMode::LocalSize,
            local_size_values.to_vec(),
        );
    }

    Ok(())
}

fn get_or_create_storage_buffer_type(
    b: &mut Builder,
    buffer_ty: &Type,
    cache: &mut HashMap<String, u32>,
) -> u32 {
    let key = format!(
        "storage:{}",
        type_cache_key(&storage_buffer_elem_type(
            buffer_ty,
            kain_core::span::Span::default()
        ))
    );
    if let Some(existing) = cache.get(&key) {
        return *existing;
    }

    let elem_type_ast = storage_buffer_elem_type(buffer_ty, kain_core::span::Span::default());
    let elem_ty = map_ast_type(b, &elem_type_ast);
    let rt_array = b.type_runtime_array(elem_ty);
    b.decorate(
        rt_array,
        Decoration::ArrayStride,
        vec![Operand::LiteralBit32(storage_buffer_stride(buffer_ty))],
    );
    let struct_ty = b.type_struct(vec![rt_array]);
    b.decorate(struct_ty, Decoration::Block, vec![]);
    b.member_decorate(
        struct_ty,
        0,
        Decoration::Offset,
        vec![Operand::LiteralBit32(0)],
    );
    cache.insert(key, struct_ty);
    struct_ty
}

fn get_or_create_uniform_wrapper_type(
    b: &mut Builder,
    uniform_ty: &Type,
    cache: &mut HashMap<String, u32>,
) -> u32 {
    let key = format!("uniform:{}", type_cache_key(uniform_ty));
    if let Some(existing) = cache.get(&key) {
        return *existing;
    }

    let inner_ty = map_ast_type(b, uniform_ty);
    let struct_ty = b.type_struct(vec![inner_ty]);
    b.decorate(struct_ty, Decoration::Block, vec![]);
    b.member_decorate(
        struct_ty,
        0,
        Decoration::Offset,
        vec![Operand::LiteralBit32(0)],
    );
    if matches!(uniform_ty, Type::Named { name, .. } if name == "Mat4") {
        b.member_decorate(struct_ty, 0, Decoration::ColMajor, vec![]);
        b.member_decorate(
            struct_ty,
            0,
            Decoration::MatrixStride,
            vec![Operand::LiteralBit32(16)],
        );
    }
    cache.insert(key, struct_ty);
    struct_ty
}

fn type_cache_key(ty: &Type) -> String {
    match ty {
        Type::Named { name, generics, .. } => {
            if generics.is_empty() {
                name.clone()
            } else {
                let inner = generics
                    .iter()
                    .map(type_cache_key)
                    .collect::<Vec<_>>()
                    .join(",");
                format!("{}<{}>", name, inner)
            }
        }
        Type::Ref { inner, .. } => format!("ref:{}", type_cache_key(inner)),
        Type::Array(inner, size, ..) => format!("array:{}:{}", type_cache_key(inner), size),
        Type::Slice(inner, ..) => format!("slice:{}", type_cache_key(inner)),
        other => format!("{:?}", other),
    }
}

fn infer_numeric_result_type(
    left: Option<&Type>,
    right: Option<&Type>,
    span: kain_core::span::Span,
) -> Option<Type> {
    let left = left?;
    let right = right?;
    let left_dim = infer_numeric_dim(left);
    let right_dim = infer_numeric_dim(right);
    if left_dim == 0 && right_dim == 0 {
        return None;
    }

    let out_dim = left_dim.max(right_dim).max(1);
    let scalar_name = if is_uint_family_name(left) || is_uint_family_name(right) {
        "UInt"
    } else if is_int_family_name(left) || is_int_family_name(right) {
        "Int"
    } else {
        "Float"
    };

    Some(if out_dim == 1 {
        Type::Named {
            name: scalar_name.into(),
            generics: vec![],
            span,
        }
    } else {
        infer_vec_type_from_scalar_name(scalar_name, out_dim, span)
    })
}

fn infer_numeric_dim(ty: &Type) -> usize {
    match ty {
        Type::Named { name, .. } => match name.as_str() {
            "Float" | "f32" | "Int" | "i32" | "UInt" | "u32" | "Bool" => 1,
            "Vec2" | "IVec2" | "UVec2" => 2,
            "Vec3" | "IVec3" | "UVec3" => 3,
            "Vec4" | "IVec4" | "UVec4" => 4,
            _ => 0,
        },
        _ => 0,
    }
}

fn is_uint_family_name(ty: &Type) -> bool {
    matches!(ty, Type::Named { name, .. } if matches!(name.as_str(), "UInt" | "u32" | "UVec2" | "UVec3" | "UVec4"))
}

fn is_int_family_name(ty: &Type) -> bool {
    matches!(ty, Type::Named { name, .. } if matches!(name.as_str(), "Int" | "i32" | "IVec2" | "IVec3" | "IVec4"))
}

fn infer_vec_type_from_scalar_name(
    scalar_name: &str,
    width: usize,
    span: kain_core::span::Span,
) -> Type {
    let name = match (scalar_name, width) {
        ("UInt", 2) => "UVec2",
        ("UInt", 3) => "UVec3",
        ("UInt", _) => "UVec4",
        ("Int", 2) => "IVec2",
        ("Int", 3) => "IVec3",
        ("Int", _) => "IVec4",
        ("Float", 2) => "Vec2",
        ("Float", 3) => "Vec3",
        _ => "Vec4",
    };
    Type::Named {
        name: name.into(),
        generics: vec![],
        span,
    }
}

fn infer_single_expr_block_type(block: &Block, known: &HashMap<String, Type>) -> Option<Type> {
    if block.stmts.len() != 1 {
        return None;
    }
    match &block.stmts[0] {
        Stmt::Expr(expr) => infer_kain_type(expr, known),
        _ => None,
    }
}

fn infer_else_branch_type(else_branch: &ElseBranch, known: &HashMap<String, Type>) -> Option<Type> {
    match else_branch {
        ElseBranch::Else(block) => infer_single_expr_block_type(block, known),
        ElseBranch::ElseIf(_, then_block, nested) => {
            let then_ty = infer_single_expr_block_type(then_block, known);
            let nested_ty = nested
                .as_deref()
                .and_then(|next| infer_else_branch_type(next, known));
            infer_numeric_result_type(then_ty.as_ref(), nested_ty.as_ref(), then_block.span)
                .or(then_ty)
                .or(nested_ty)
        }
    }
}

impl<'a> ShaderContext<'a> {
    fn get_glsl_ext(&mut self) -> u32 {
        if let Some(ext) = self.glsl_ext {
            ext
        } else {
            let ext = self.b.ext_inst_import("GLSL.std.450");
            self.glsl_ext = Some(ext);
            ext
        }
    }
}

/// Pure AST type inference — no Builder calls. Returns KAIN Type for a let-binding RHS.
/// Uses a known-names map to resolve Ident references from previously seen let-bindings.
fn infer_kain_type(expr: &Expr, known: &HashMap<String, Type>) -> Option<Type> {
    let span = expr.span();
    let named = |n: &str| -> Type {
        Type::Named {
            name: n.into(),
            generics: vec![],
            span,
        }
    };

    match expr {
        Expr::Float(_, _) => Some(named("Float")),
        Expr::Int(_, _) => Some(named("Int")),
        Expr::Bool(_, _) => Some(named("Bool")),
        Expr::Ident(name, _) => known.get(name.as_str()).cloned(),
        Expr::Unary { operand, .. } => infer_kain_type(operand, known),
        Expr::Paren(inner, _) => infer_kain_type(inner, known),
        Expr::Binary {
            left, op, right, ..
        } => match op {
            BinaryOp::Eq
            | BinaryOp::Ne
            | BinaryOp::Lt
            | BinaryOp::Le
            | BinaryOp::Gt
            | BinaryOp::Ge
            | BinaryOp::And
            | BinaryOp::Or => Some(named("Bool")),
            _ => {
                let left_ty = infer_kain_type(left, known);
                let right_ty = infer_kain_type(right, known);
                infer_numeric_result_type(left_ty.as_ref(), right_ty.as_ref(), span)
                    .or(left_ty)
                    .or(right_ty)
            }
        },
        Expr::If {
            then_branch,
            else_branch,
            ..
        } => {
            let then_ty = infer_single_expr_block_type(then_branch, known);
            let else_ty = else_branch
                .as_deref()
                .and_then(|eb| infer_else_branch_type(eb, known));
            infer_numeric_result_type(then_ty.as_ref(), else_ty.as_ref(), span)
                .or(then_ty)
                .or(else_ty)
        }
        Expr::Cast { target, .. } => Some(target.clone()),
        Expr::Field { object, field, .. } => {
            // Derive element type from parent vector type.
            let parent_ty = infer_kain_type(object, known);
            let elem_scalar = match parent_ty.as_ref().and_then(|t| {
                if let Type::Named { name, .. } = t {
                    Some(name.as_str())
                } else {
                    None
                }
            }) {
                Some("IVec2" | "IVec3" | "IVec4") => "Int",
                Some("UVec2" | "UVec3" | "UVec4") => "UInt",
                _ => "Float", // Vec2/3/4 and scalars default to Float
            };
            let elem_vec2 = match elem_scalar {
                "Int" => "IVec2",
                "UInt" => "UVec2",
                _ => "Vec2",
            };
            let elem_vec3 = match elem_scalar {
                "Int" => "IVec3",
                "UInt" => "UVec3",
                _ => "Vec3",
            };
            let elem_vec4 = match elem_scalar {
                "Int" => "IVec4",
                "UInt" => "UVec4",
                _ => "Vec4",
            };
            match field.len() {
                1 => Some(named(elem_scalar)),
                2 => Some(named(elem_vec2)),
                3 => Some(named(elem_vec3)),
                4 => Some(named(elem_vec4)),
                _ => parent_ty,
            }
        }
        Expr::Call { callee, args, .. } => {
            if let Expr::Ident(fn_name, _) = callee.as_ref() {
                match (fn_name.as_str(), args.len()) {
                    // Constructors
                    ("vec2" | "Vec2", 2) => Some(named("Vec2")),
                    ("vec3" | "Vec3", 3) => Some(named("Vec3")),
                    ("vec4" | "Vec4", 4) => Some(named("Vec4")),
                    ("ivec2" | "IVec2", 2) => Some(named("IVec2")),
                    ("ivec3" | "IVec3", 3) => Some(named("IVec3")),
                    ("ivec4" | "IVec4", 4) => Some(named("IVec4")),
                    ("uvec2" | "UVec2", 2) => Some(named("UVec2")),
                    ("uvec3" | "UVec3", 3) => Some(named("UVec3")),
                    ("uvec4" | "UVec4", 4) => Some(named("UVec4")),
                    ("Float", 1) => Some(named("Float")),
                    ("Int", 1) => Some(named("Int")),
                    ("UInt", 1) => Some(named("UInt")),
                    ("Bool", 1) => Some(named("Bool")),
                    // Scalar math (same type as first arg)
                    (
                        "sin" | "cos" | "tan" | "asin" | "acos" | "atan" | "sqrt" | "abs" | "floor"
                        | "ceil" | "fract" | "round" | "trunc" | "sign" | "exp" | "log" | "exp2"
                        | "log2" | "inversesqrt" | "radians" | "degrees",
                        1,
                    ) => infer_kain_type(&args[0].value, known),
                    ("pow" | "atan2" | "min" | "max" | "step", 2) => {
                        infer_kain_type(&args[0].value, known)
                    }
                    ("clamp" | "mix" | "smoothstep", 3) => infer_kain_type(&args[0].value, known),
                    // Scalar reductions
                    ("length" | "distance" | "dot", _) => Some(named("Float")),
                    // Vector-preserving
                    ("normalize" | "cross" | "reflect", _) => {
                        infer_kain_type(&args[0].value, known)
                    }
                    ("refract", 3) => infer_kain_type(&args[0].value, known),
                    // Texture
                    ("sample" | "sample_lod", _) => Some(named("Vec4")),
                    _ => None,
                }
            } else {
                None
            }
        }
        Expr::Index { object, .. } => {
            let object_ty = infer_kain_type(object, known)?;
            match object_ty {
                Type::Named { name, generics, .. } if name == "StorageBuffer" => {
                    generics.first().cloned().or_else(|| Some(named("Float")))
                }
                Type::Array(inner, _, _) | Type::Slice(inner, _) => Some((*inner).clone()),
                _ => None,
            }
        }
        _ => None,
    }
}

/// Pure AST walk — collects (binding_name, inferred_KainType) for every let in the block tree.
/// No Builder interaction at all. Unknown types are mapped to Float as safe fallback.
fn collect_binding_types(block: &Block, out: &mut Vec<(String, Type)>) {
    let known: HashMap<String, Type> = HashMap::new();
    collect_block_types(block, &mut { known }, out);
}

fn collect_binding_types_seeded(
    block: &Block,
    seed: HashMap<String, Type>,
    out: &mut Vec<(String, Type)>,
) {
    collect_block_types(block, &mut { seed }, out);
}

fn internal_hoist_name(prefix: &str, span: kain_core::span::Span) -> String {
    format!("__{}_{}_{}", prefix, span.start, span.end)
}

fn collect_block_types(
    block: &Block,
    known: &mut HashMap<String, Type>,
    out: &mut Vec<(String, Type)>,
) {
    let float_ty = |span: kain_core::span::Span| Type::Named {
        name: "Float".into(),
        generics: vec![],
        span,
    };
    let int_ty = |span: kain_core::span::Span| Type::Named {
        name: "Int".into(),
        generics: vec![],
        span,
    };

    for stmt in &block.stmts {
        match stmt {
            Stmt::Let { pattern, value, .. } => {
                if let (kain_core::ast::Pattern::Binding { name, .. }, Some(value)) =
                    (pattern, value)
                {
                    let span = value.span();
                    let ty = infer_kain_type(value, known).unwrap_or_else(|| float_ty(span));
                    known.insert(name.clone(), ty.clone());
                    out.push((name.clone(), ty));
                }
            }
            Stmt::Expr(Expr::If {
                then_branch,
                else_branch,
                ..
            }) => {
                collect_block_types(then_branch, known, out);
                if let Some(eb) = else_branch {
                    match eb.as_ref() {
                        ElseBranch::Else(blk) => collect_block_types(blk, known, out),
                        ElseBranch::ElseIf(_, blk, next) => {
                            collect_block_types(blk, known, out);
                            if let Some(nb) = next {
                                if let ElseBranch::Else(b2) = nb.as_ref() {
                                    collect_block_types(b2, known, out);
                                }
                            }
                        }
                    }
                }
            }
            Stmt::While { body, .. } => collect_block_types(body, known, out),
            Stmt::For { body, span, .. } => {
                out.push((internal_hoist_name("for_iter", *span), int_ty(*span)));
                out.push((internal_hoist_name("for_end", *span), int_ty(*span)));
                collect_block_types(body, known, out)
            }
            Stmt::Loop { body, .. } => collect_block_types(body, known, out),
            _ => {} // Ignore others for now
        }
    }
}

fn take_hoisted_slot(ctx: &mut ShaderContext, name: &str, ptr_ty: u32) -> Option<u32> {
    ctx.hoisted_vars.get_mut(name).and_then(|slots| {
        let idx = slots
            .iter()
            .position(|(_, hoisted_ptr)| *hoisted_ptr == ptr_ty)?;
        Some(slots.remove(idx).0)
    })
}

fn emit_block(ctx: &mut ShaderContext, block: &Block) -> KainResult<()> {
    for stmt in &block.stmts {
        match stmt {
            Stmt::Return(expr, _) => {
                if let Some(expr) = expr {
                    if let Some(out_var) = ctx.output_var {
                        let (val, _) = emit_expr(ctx, expr)?;
                        ctx.b.store(out_var, val, None, vec![]).unwrap();
                    }
                }
                ctx.b.ret().unwrap();
            }
            Stmt::Let { pattern, value, .. } => {
                if let Some(value) = value {
                    let (val, ty) = emit_expr(ctx, value)?;
                    if let kain_core::ast::Pattern::Binding { name, .. } = pattern {
                        // Look up the pre-hoisted OpVariable slot (emitted at function top).
                        // Validate the slot type matches the actual value type — the hoist
                        // pre-pass may have used a float fallback for Ident-typed RHS expressions
                        // it couldn't statically resolve (e.g. `refract(i_vec, n_vec, eta)`).
                        // If there's a mismatch, emit a new correctly-typed variable here.
                        let type_id = map_ast_type(ctx.b, &ty);
                        let ptr_ty = ctx.b.type_pointer(None, StorageClass::Function, type_id);
                        let local_var = take_hoisted_slot(ctx, name, ptr_ty).unwrap_or_else(|| {
                            ctx.b.variable(ptr_ty, None, StorageClass::Function, None)
                        });
                        ctx.b
                            .store(local_var, val, None, std::iter::empty())
                            .unwrap();
                        ctx.vars.insert(
                            name.clone(),
                            VarBinding {
                                id: local_var,
                                ty,
                                is_ptr: true,
                            },
                        );
                    }
                }
            }
            Stmt::Expr(expr) => {
                if let Expr::If {
                    condition,
                    then_branch,
                    else_branch,
                    ..
                } = expr
                {
                    emit_if_statement(ctx, condition, then_branch, else_branch.as_deref())?;
                } else {
                    emit_expr(ctx, expr)?;
                }
            }
            Stmt::While {
                condition, body, ..
            } => {
                emit_while_statement(ctx, condition, body)?;
            }
            Stmt::For {
                binding,
                iter,
                body,
                span,
            } => {
                emit_for_statement(ctx, binding, iter, body, *span)?;
            }
            Stmt::Break(_, span) => {
                let break_target = ctx
                    .loop_break_targets
                    .last()
                    .copied()
                    .ok_or_else(|| KainError::codegen("break outside loop", *span))?;
                terminate_with_branch(ctx, break_target)?;
                let cont_label = ctx.b.id();
                ctx.b.begin_block(Some(cont_label)).unwrap();
            }
            Stmt::Continue(span) => {
                let continue_target = ctx
                    .loop_continue_targets
                    .last()
                    .copied()
                    .ok_or_else(|| KainError::codegen("continue outside loop", *span))?;
                terminate_with_branch(ctx, continue_target)?;
                let cont_label = ctx.b.id();
                ctx.b.begin_block(Some(cont_label)).unwrap();
            }
            _ => {} // Ignore others for now
        }
    }
    Ok(())
}

fn emit_expr(ctx: &mut ShaderContext, expr: &Expr) -> KainResult<(u32, Type)> {
    match expr {
        Expr::Ident(name, span) => {
            if let Some(binding) = ctx.vars.get(name).cloned() {
                if binding.is_ptr {
                    if is_storage_buffer(&binding.ty) {
                        // Storage buffers are pointer-backed structs and must be consumed via indexing.
                        return Ok((binding.id, binding.ty));
                    }
                    // Need to load from pointer
                    let type_id = map_ast_type(ctx.b, &binding.ty);

                    // Check if this is a struct-wrapped uniform
                    if ctx.struct_uniforms.contains(name) {
                        // Use AccessChain to get pointer to member 0 of the struct
                        let ptr_ty = ctx.b.type_pointer(None, StorageClass::Uniform, type_id);
                        let int_ty = ctx.b.type_int(32, 0);
                        let zero = ctx.b.constant_bit32(int_ty, 0);
                        let member_ptr = ctx
                            .b
                            .access_chain(ptr_ty, None, binding.id, vec![zero])
                            .unwrap();
                        let val_id = ctx
                            .b
                            .load(type_id, None, member_ptr, None, std::iter::empty())
                            .unwrap();
                        Ok((val_id, binding.ty))
                    } else {
                        // Direct load for inputs and non-wrapped uniforms
                        let val_id = ctx
                            .b
                            .load(type_id, None, binding.id, None, std::iter::empty())
                            .unwrap();
                        Ok((val_id, binding.ty))
                    }
                } else {
                    Ok((binding.id, binding.ty))
                }
            } else {
                Err(KainError::codegen(
                    format!("Unknown variable: {}", name),
                    *span,
                ))
            }
        }
        Expr::Binary {
            left, op, right, ..
        } => {
            let (lhs, lhs_ty) = emit_expr(ctx, left)?;
            let (rhs, rhs_ty) = emit_expr(ctx, right)?;

            // Map types to SPIR-V types
            let res_ty_id = map_ast_type(ctx.b, &lhs_ty);

            let mut result_ty_override: Option<Type> = None;
            let res_id = match op {
                BinaryOp::Mul => {
                    if is_mat4(&lhs_ty) && is_mat4(&rhs_ty) {
                        ctx.b
                            .matrix_times_matrix(res_ty_id, None, lhs, rhs)
                            .unwrap()
                    } else if is_mat4(&lhs_ty) && is_vec4(&rhs_ty) {
                        // Mat4 * Vec4 -> Vec4
                        let vec4_ty = map_ast_type(ctx.b, &rhs_ty);
                        ctx.b.matrix_times_vector(vec4_ty, None, lhs, rhs).unwrap()
                    } else if is_vec4(&lhs_ty) && is_mat4(&rhs_ty) {
                        // Vec4 * Mat4 -> Vec4
                        let vec4_ty = map_ast_type(ctx.b, &lhs_ty);
                        ctx.b.vector_times_matrix(vec4_ty, None, lhs, rhs).unwrap()
                    } else if is_float(&lhs_ty) && is_float(&rhs_ty) {
                        ctx.b.f_mul(res_ty_id, None, lhs, rhs).unwrap()
                    } else if is_uint(&lhs_ty) || is_uint(&rhs_ty) {
                        let uint_ty = ctx.b.type_int(32, 0);
                        let lhs_u = cast_to_u32(ctx, lhs, &lhs_ty);
                        let rhs_u = cast_to_u32(ctx, rhs, &rhs_ty);
                        result_ty_override = Some(Type::Named {
                            name: "UInt".into(),
                            generics: vec![],
                            span: expr.span(),
                        });
                        ctx.b.i_mul(uint_ty, None, lhs_u, rhs_u).unwrap()
                    } else if is_int(&lhs_ty) || is_int(&rhs_ty) {
                        let int_ty = ctx.b.type_int(32, 1);
                        let lhs_i = cast_to_i32(ctx, lhs, &lhs_ty);
                        let rhs_i = cast_to_i32(ctx, rhs, &rhs_ty);
                        result_ty_override = Some(Type::Named {
                            name: "Int".into(),
                            generics: vec![],
                            span: expr.span(),
                        });
                        ctx.b.i_mul(int_ty, None, lhs_i, rhs_i).unwrap()
                    } else {
                        // Float fallback with explicit vector-scalar harmonization.
                        let (lhs_f, rhs_f, out_ty) =
                            coerce_float_binary_operands(ctx, lhs, &lhs_ty, rhs, &rhs_ty);
                        result_ty_override = Some(out_ty.clone());
                        let out_ty_id = map_ast_type(ctx.b, &out_ty);
                        ctx.b.f_mul(out_ty_id, None, lhs_f, rhs_f).unwrap()
                    }
                }
                BinaryOp::Add => {
                    if is_uint(&lhs_ty) || is_uint(&rhs_ty) {
                        let uint_ty = ctx.b.type_int(32, 0);
                        let lhs_u = cast_to_u32(ctx, lhs, &lhs_ty);
                        let rhs_u = cast_to_u32(ctx, rhs, &rhs_ty);
                        result_ty_override = Some(Type::Named {
                            name: "UInt".into(),
                            generics: vec![],
                            span: expr.span(),
                        });
                        ctx.b.i_add(uint_ty, None, lhs_u, rhs_u).unwrap()
                    } else if is_int(&lhs_ty) || is_int(&rhs_ty) {
                        let int_ty = ctx.b.type_int(32, 1);
                        let lhs_i = cast_to_i32(ctx, lhs, &lhs_ty);
                        let rhs_i = cast_to_i32(ctx, rhs, &rhs_ty);
                        result_ty_override = Some(Type::Named {
                            name: "Int".into(),
                            generics: vec![],
                            span: expr.span(),
                        });
                        ctx.b.i_add(int_ty, None, lhs_i, rhs_i).unwrap()
                    } else {
                        let (lhs_f, rhs_f, out_ty) =
                            coerce_float_binary_operands(ctx, lhs, &lhs_ty, rhs, &rhs_ty);
                        result_ty_override = Some(out_ty.clone());
                        let out_ty_id = map_ast_type(ctx.b, &out_ty);
                        ctx.b.f_add(out_ty_id, None, lhs_f, rhs_f).unwrap()
                    }
                }
                BinaryOp::Sub => {
                    if is_uint(&lhs_ty) || is_uint(&rhs_ty) {
                        let uint_ty = ctx.b.type_int(32, 0);
                        let lhs_u = cast_to_u32(ctx, lhs, &lhs_ty);
                        let rhs_u = cast_to_u32(ctx, rhs, &rhs_ty);
                        result_ty_override = Some(Type::Named {
                            name: "UInt".into(),
                            generics: vec![],
                            span: expr.span(),
                        });
                        ctx.b.i_sub(uint_ty, None, lhs_u, rhs_u).unwrap()
                    } else if is_int(&lhs_ty) || is_int(&rhs_ty) {
                        let int_ty = ctx.b.type_int(32, 1);
                        let lhs_i = cast_to_i32(ctx, lhs, &lhs_ty);
                        let rhs_i = cast_to_i32(ctx, rhs, &rhs_ty);
                        result_ty_override = Some(Type::Named {
                            name: "Int".into(),
                            generics: vec![],
                            span: expr.span(),
                        });
                        ctx.b.i_sub(int_ty, None, lhs_i, rhs_i).unwrap()
                    } else {
                        let (lhs_f, rhs_f, out_ty) =
                            coerce_float_binary_operands(ctx, lhs, &lhs_ty, rhs, &rhs_ty);
                        result_ty_override = Some(out_ty.clone());
                        let out_ty_id = map_ast_type(ctx.b, &out_ty);
                        ctx.b.f_sub(out_ty_id, None, lhs_f, rhs_f).unwrap()
                    }
                }
                BinaryOp::Div => {
                    if is_uint(&lhs_ty) || is_uint(&rhs_ty) {
                        let uint_ty = ctx.b.type_int(32, 0);
                        let lhs_u = cast_to_u32(ctx, lhs, &lhs_ty);
                        let rhs_u = cast_to_u32(ctx, rhs, &rhs_ty);
                        result_ty_override = Some(Type::Named {
                            name: "UInt".into(),
                            generics: vec![],
                            span: expr.span(),
                        });
                        ctx.b.u_div(uint_ty, None, lhs_u, rhs_u).unwrap()
                    } else if is_int(&lhs_ty) || is_int(&rhs_ty) {
                        let int_ty = ctx.b.type_int(32, 1);
                        let lhs_i = cast_to_i32(ctx, lhs, &lhs_ty);
                        let rhs_i = cast_to_i32(ctx, rhs, &rhs_ty);
                        result_ty_override = Some(Type::Named {
                            name: "Int".into(),
                            generics: vec![],
                            span: expr.span(),
                        });
                        ctx.b.s_div(int_ty, None, lhs_i, rhs_i).unwrap()
                    } else {
                        let (lhs_f, rhs_f, out_ty) =
                            coerce_float_binary_operands(ctx, lhs, &lhs_ty, rhs, &rhs_ty);
                        result_ty_override = Some(out_ty.clone());
                        let out_ty_id = map_ast_type(ctx.b, &out_ty);
                        ctx.b.f_div(out_ty_id, None, lhs_f, rhs_f).unwrap()
                    }
                }
                BinaryOp::Mod => {
                    if is_uint(&lhs_ty) || is_uint(&rhs_ty) {
                        let uint_ty = ctx.b.type_int(32, 0);
                        let lhs_u = cast_to_u32(ctx, lhs, &lhs_ty);
                        let rhs_u = cast_to_u32(ctx, rhs, &rhs_ty);
                        result_ty_override = Some(Type::Named {
                            name: "UInt".into(),
                            generics: vec![],
                            span: expr.span(),
                        });
                        ctx.b.u_mod(uint_ty, None, lhs_u, rhs_u).unwrap()
                    } else if is_int(&lhs_ty) || is_int(&rhs_ty) {
                        let int_ty = ctx.b.type_int(32, 1);
                        let lhs_i = cast_to_i32(ctx, lhs, &lhs_ty);
                        let rhs_i = cast_to_i32(ctx, rhs, &rhs_ty);
                        result_ty_override = Some(Type::Named {
                            name: "Int".into(),
                            generics: vec![],
                            span: expr.span(),
                        });
                        ctx.b.s_mod(int_ty, None, lhs_i, rhs_i).unwrap()
                    } else {
                        let (lhs_f, rhs_f, out_ty) =
                            coerce_float_binary_operands(ctx, lhs, &lhs_ty, rhs, &rhs_ty);
                        result_ty_override = Some(out_ty.clone());
                        let out_ty_id = map_ast_type(ctx.b, &out_ty);
                        ctx.b.f_mod(out_ty_id, None, lhs_f, rhs_f).unwrap()
                    }
                }
                BinaryOp::Pow => {
                    let glsl = ctx.get_glsl_ext();
                    ctx.b
                        .ext_inst(
                            res_ty_id,
                            None,
                            glsl,
                            26,
                            vec![Operand::IdRef(lhs), Operand::IdRef(rhs)],
                        )
                        .unwrap()
                }
                BinaryOp::Eq => {
                    let bool_ty = ctx.b.type_bool();
                    if is_float(&lhs_ty) && is_float(&rhs_ty) {
                        ctx.b.f_ord_equal(bool_ty, None, lhs, rhs).unwrap()
                    } else {
                        ctx.b.i_equal(bool_ty, None, lhs, rhs).unwrap()
                    }
                }
                BinaryOp::Ne => {
                    let bool_ty = ctx.b.type_bool();
                    if is_float(&lhs_ty) && is_float(&rhs_ty) {
                        ctx.b.f_ord_not_equal(bool_ty, None, lhs, rhs).unwrap()
                    } else {
                        ctx.b.i_not_equal(bool_ty, None, lhs, rhs).unwrap()
                    }
                }
                BinaryOp::Lt => {
                    let bool_ty = ctx.b.type_bool();
                    if is_float(&lhs_ty) && is_float(&rhs_ty) {
                        ctx.b.f_ord_less_than(bool_ty, None, lhs, rhs).unwrap()
                    } else if is_uint(&lhs_ty) && is_uint(&rhs_ty) {
                        ctx.b.u_less_than(bool_ty, None, lhs, rhs).unwrap()
                    } else {
                        ctx.b.s_less_than(bool_ty, None, lhs, rhs).unwrap()
                    }
                }
                BinaryOp::Le => {
                    let bool_ty = ctx.b.type_bool();
                    if is_float(&lhs_ty) && is_float(&rhs_ty) {
                        ctx.b
                            .f_ord_less_than_equal(bool_ty, None, lhs, rhs)
                            .unwrap()
                    } else if is_uint(&lhs_ty) && is_uint(&rhs_ty) {
                        ctx.b.u_less_than_equal(bool_ty, None, lhs, rhs).unwrap()
                    } else {
                        ctx.b.s_less_than_equal(bool_ty, None, lhs, rhs).unwrap()
                    }
                }
                BinaryOp::Gt => {
                    let bool_ty = ctx.b.type_bool();
                    if is_float(&lhs_ty) && is_float(&rhs_ty) {
                        ctx.b.f_ord_greater_than(bool_ty, None, lhs, rhs).unwrap()
                    } else if is_uint(&lhs_ty) && is_uint(&rhs_ty) {
                        ctx.b.u_greater_than(bool_ty, None, lhs, rhs).unwrap()
                    } else {
                        ctx.b.s_greater_than(bool_ty, None, lhs, rhs).unwrap()
                    }
                }
                BinaryOp::Ge => {
                    let bool_ty = ctx.b.type_bool();
                    if is_float(&lhs_ty) && is_float(&rhs_ty) {
                        ctx.b
                            .f_ord_greater_than_equal(bool_ty, None, lhs, rhs)
                            .unwrap()
                    } else if is_uint(&lhs_ty) && is_uint(&rhs_ty) {
                        ctx.b.u_greater_than_equal(bool_ty, None, lhs, rhs).unwrap()
                    } else {
                        ctx.b.s_greater_than_equal(bool_ty, None, lhs, rhs).unwrap()
                    }
                }
                BinaryOp::And => {
                    let bool_ty = ctx.b.type_bool();
                    ctx.b.logical_and(bool_ty, None, lhs, rhs).unwrap()
                }
                BinaryOp::Or => {
                    let bool_ty = ctx.b.type_bool();
                    ctx.b.logical_or(bool_ty, None, lhs, rhs).unwrap()
                }
                BinaryOp::BitAnd => ctx.b.bitwise_and(res_ty_id, None, lhs, rhs).unwrap(),
                BinaryOp::BitOr => ctx.b.bitwise_or(res_ty_id, None, lhs, rhs).unwrap(),
                BinaryOp::BitXor => ctx.b.bitwise_xor(res_ty_id, None, lhs, rhs).unwrap(),
                BinaryOp::Shl => ctx.b.shift_left_logical(res_ty_id, None, lhs, rhs).unwrap(),
                BinaryOp::Shr => {
                    if is_uint(&lhs_ty) {
                        ctx.b
                            .shift_right_logical(res_ty_id, None, lhs, rhs)
                            .unwrap()
                    } else {
                        ctx.b
                            .shift_right_arithmetic(res_ty_id, None, lhs, rhs)
                            .unwrap()
                    }
                }
                _ => {
                    return Err(KainError::codegen(
                        "Unsupported binary op in shader",
                        expr.span(),
                    ))
                }
            };

            // Result type inference
            let res_ty = if let Some(override_ty) = result_ty_override {
                override_ty
            } else if matches!(
                op,
                BinaryOp::Eq
                    | BinaryOp::Ne
                    | BinaryOp::Lt
                    | BinaryOp::Le
                    | BinaryOp::Gt
                    | BinaryOp::Ge
                    | BinaryOp::And
                    | BinaryOp::Or
            ) {
                Type::Named {
                    name: "Bool".into(),
                    generics: vec![],
                    span: expr.span(),
                }
            } else if is_mat4(&lhs_ty) && is_vec4(&rhs_ty) {
                rhs_ty
            } else {
                lhs_ty
            };

            Ok((res_id, res_ty))
        }
        Expr::Call { callee, args, .. } => {
            if let Expr::Ident(name, _) = &**callee {
                let float = ctx.b.type_float(32);
                let int = ctx.b.type_int(32, 1);
                let uint = ctx.b.type_int(32, 0);

                // Vector constructors
                match name.as_str() {
                    "vec2" | "Vec2" if args.len() == 2 => {
                        let vec2 = ctx.b.type_vector(float, 2);
                        let mut components = vec![];
                        for arg in args {
                            let (val, _) = emit_expr(ctx, &arg.value)?;
                            components.push(val);
                        }
                        let res_id = ctx.b.composite_construct(vec2, None, components).unwrap();
                        return Ok((
                            res_id,
                            Type::Named {
                                name: "Vec2".into(),
                                generics: vec![],
                                span: expr.span(),
                            },
                        ));
                    }
                    "vec3" | "Vec3" if args.len() == 3 => {
                        let vec3 = ctx.b.type_vector(float, 3);
                        let mut components = vec![];
                        for arg in args {
                            let (val, _) = emit_expr(ctx, &arg.value)?;
                            components.push(val);
                        }
                        let res_id = ctx.b.composite_construct(vec3, None, components).unwrap();
                        return Ok((
                            res_id,
                            Type::Named {
                                name: "Vec3".into(),
                                generics: vec![],
                                span: expr.span(),
                            },
                        ));
                    }
                    "vec4" | "Vec4" if args.len() == 4 => {
                        let vec4 = ctx.b.type_vector(float, 4);
                        let mut components = vec![];
                        for arg in args {
                            let (val, _) = emit_expr(ctx, &arg.value)?;
                            components.push(val);
                        }
                        let res_id = ctx.b.composite_construct(vec4, None, components).unwrap();
                        return Ok((
                            res_id,
                            Type::Named {
                                name: "Vec4".into(),
                                generics: vec![],
                                span: expr.span(),
                            },
                        ));
                    }
                    "uvec2" | "UVec2" if args.len() == 2 => {
                        let uvec2 = ctx.b.type_vector(uint, 2);
                        let mut components = vec![];
                        for arg in args {
                            let (val, _) = emit_expr(ctx, &arg.value)?;
                            components.push(val);
                        }
                        let res_id = ctx.b.composite_construct(uvec2, None, components).unwrap();
                        return Ok((
                            res_id,
                            Type::Named {
                                name: "UVec2".into(),
                                generics: vec![],
                                span: expr.span(),
                            },
                        ));
                    }
                    "uvec3" | "UVec3" if args.len() == 3 => {
                        let uvec3 = ctx.b.type_vector(uint, 3);
                        let mut components = vec![];
                        for arg in args {
                            let (val, _) = emit_expr(ctx, &arg.value)?;
                            components.push(val);
                        }
                        let res_id = ctx.b.composite_construct(uvec3, None, components).unwrap();
                        return Ok((
                            res_id,
                            Type::Named {
                                name: "UVec3".into(),
                                generics: vec![],
                                span: expr.span(),
                            },
                        ));
                    }
                    "uvec4" | "UVec4" if args.len() == 4 => {
                        let uvec4 = ctx.b.type_vector(uint, 4);
                        let mut components = vec![];
                        for arg in args {
                            let (val, _) = emit_expr(ctx, &arg.value)?;
                            components.push(val);
                        }
                        let res_id = ctx.b.composite_construct(uvec4, None, components).unwrap();
                        return Ok((
                            res_id,
                            Type::Named {
                                name: "UVec4".into(),
                                generics: vec![],
                                span: expr.span(),
                            },
                        ));
                    }
                    "ivec2" | "IVec2" if args.len() == 2 => {
                        let ivec2 = ctx.b.type_vector(int, 2);
                        let mut components = vec![];
                        for arg in args {
                            let (val, _) = emit_expr(ctx, &arg.value)?;
                            components.push(val);
                        }
                        let res_id = ctx.b.composite_construct(ivec2, None, components).unwrap();
                        return Ok((
                            res_id,
                            Type::Named {
                                name: "IVec2".into(),
                                generics: vec![],
                                span: expr.span(),
                            },
                        ));
                    }
                    "ivec3" | "IVec3" if args.len() == 3 => {
                        let ivec3 = ctx.b.type_vector(int, 3);
                        let mut components = vec![];
                        for arg in args {
                            let (val, _) = emit_expr(ctx, &arg.value)?;
                            components.push(val);
                        }
                        let res_id = ctx.b.composite_construct(ivec3, None, components).unwrap();
                        return Ok((
                            res_id,
                            Type::Named {
                                name: "IVec3".into(),
                                generics: vec![],
                                span: expr.span(),
                            },
                        ));
                    }
                    "ivec4" | "IVec4" if args.len() == 4 => {
                        let ivec4 = ctx.b.type_vector(int, 4);
                        let mut components = vec![];
                        for arg in args {
                            let (val, _) = emit_expr(ctx, &arg.value)?;
                            components.push(val);
                        }
                        let res_id = ctx.b.composite_construct(ivec4, None, components).unwrap();
                        return Ok((
                            res_id,
                            Type::Named {
                                name: "IVec4".into(),
                                generics: vec![],
                                span: expr.span(),
                            },
                        ));
                    }

                    // Scalar constructors/casts (Float(x), Int(x), UInt(x), Bool(x))
                    "float" | "Float" if args.len() == 1 => {
                        let cast_expr = Expr::Cast {
                            value: Box::new(args[0].value.clone()),
                            target: Type::Named {
                                name: "Float".into(),
                                generics: vec![],
                                span: expr.span(),
                            },
                            span: expr.span(),
                        };
                        return emit_expr(ctx, &cast_expr);
                    }
                    "int" | "Int" if args.len() == 1 => {
                        let cast_expr = Expr::Cast {
                            value: Box::new(args[0].value.clone()),
                            target: Type::Named {
                                name: "Int".into(),
                                generics: vec![],
                                span: expr.span(),
                            },
                            span: expr.span(),
                        };
                        return emit_expr(ctx, &cast_expr);
                    }
                    "uint" | "UInt" if args.len() == 1 => {
                        let cast_expr = Expr::Cast {
                            value: Box::new(args[0].value.clone()),
                            target: Type::Named {
                                name: "UInt".into(),
                                generics: vec![],
                                span: expr.span(),
                            },
                            span: expr.span(),
                        };
                        return emit_expr(ctx, &cast_expr);
                    }
                    "bool" | "Bool" if args.len() == 1 => {
                        let cast_expr = Expr::Cast {
                            value: Box::new(args[0].value.clone()),
                            target: Type::Named {
                                name: "Bool".into(),
                                generics: vec![],
                                span: expr.span(),
                            },
                            span: expr.span(),
                        };
                        return emit_expr(ctx, &cast_expr);
                    }

                    // Math functions (GLSL extended instructions)
                    "sin" if args.len() == 1 => {
                        let (val, ty) = emit_expr(ctx, &args[0].value)?;
                        let res_ty = map_ast_type(ctx.b, &ty);
                        let glsl = ctx.get_glsl_ext();
                        let res_id = ctx
                            .b
                            .ext_inst(res_ty, None, glsl, 13, vec![Operand::IdRef(val)])
                            .unwrap(); // Sin = 13
                        return Ok((res_id, ty));
                    }
                    "cos" if args.len() == 1 => {
                        let (val, ty) = emit_expr(ctx, &args[0].value)?;
                        let res_ty = map_ast_type(ctx.b, &ty);
                        let glsl = ctx.get_glsl_ext();
                        let res_id = ctx
                            .b
                            .ext_inst(res_ty, None, glsl, 14, vec![Operand::IdRef(val)])
                            .unwrap(); // Cos = 14
                        return Ok((res_id, ty));
                    }
                    "tan" if args.len() == 1 => {
                        let (val, ty) = emit_expr(ctx, &args[0].value)?;
                        let res_ty = map_ast_type(ctx.b, &ty);
                        let glsl = ctx.get_glsl_ext();
                        let res_id = ctx
                            .b
                            .ext_inst(res_ty, None, glsl, 15, vec![Operand::IdRef(val)])
                            .unwrap(); // Tan = 15
                        return Ok((res_id, ty));
                    }
                    "pow" if args.len() == 2 => {
                        let (base, ty) = emit_expr(ctx, &args[0].value)?;
                        let (exp, _) = emit_expr(ctx, &args[1].value)?;
                        let res_ty = map_ast_type(ctx.b, &ty);
                        let glsl = ctx.get_glsl_ext();
                        let res_id = ctx
                            .b
                            .ext_inst(
                                res_ty,
                                None,
                                glsl,
                                26,
                                vec![Operand::IdRef(base), Operand::IdRef(exp)],
                            )
                            .unwrap(); // Pow = 26
                        return Ok((res_id, ty));
                    }
                    "sqrt" if args.len() == 1 => {
                        let (val, ty) = emit_expr(ctx, &args[0].value)?;
                        let res_ty = map_ast_type(ctx.b, &ty);
                        let glsl = ctx.get_glsl_ext();
                        let res_id = ctx
                            .b
                            .ext_inst(res_ty, None, glsl, 31, vec![Operand::IdRef(val)])
                            .unwrap(); // Sqrt = 31
                        return Ok((res_id, ty));
                    }
                    "abs" if args.len() == 1 => {
                        let (val, ty) = emit_expr(ctx, &args[0].value)?;
                        let res_ty = map_ast_type(ctx.b, &ty);
                        let glsl = ctx.get_glsl_ext();
                        let res_id = ctx
                            .b
                            .ext_inst(res_ty, None, glsl, 4, vec![Operand::IdRef(val)])
                            .unwrap(); // FAbs = 4
                        return Ok((res_id, ty));
                    }
                    "floor" if args.len() == 1 => {
                        let (val, ty) = emit_expr(ctx, &args[0].value)?;
                        let res_ty = map_ast_type(ctx.b, &ty);
                        let glsl = ctx.get_glsl_ext();
                        let res_id = ctx
                            .b
                            .ext_inst(res_ty, None, glsl, 8, vec![Operand::IdRef(val)])
                            .unwrap(); // Floor = 8
                        return Ok((res_id, ty));
                    }
                    "ceil" if args.len() == 1 => {
                        let (val, ty) = emit_expr(ctx, &args[0].value)?;
                        let res_ty = map_ast_type(ctx.b, &ty);
                        let glsl = ctx.get_glsl_ext();
                        let res_id = ctx
                            .b
                            .ext_inst(res_ty, None, glsl, 9, vec![Operand::IdRef(val)])
                            .unwrap(); // Ceil = 9
                        return Ok((res_id, ty));
                    }
                    "fract" if args.len() == 1 => {
                        let (val, ty) = emit_expr(ctx, &args[0].value)?;
                        let res_ty = map_ast_type(ctx.b, &ty);
                        let glsl = ctx.get_glsl_ext();
                        let res_id = ctx
                            .b
                            .ext_inst(res_ty, None, glsl, 10, vec![Operand::IdRef(val)])
                            .unwrap(); // Fract = 10
                        return Ok((res_id, ty));
                    }
                    "min" if args.len() == 2 => {
                        let (a, ty) = emit_expr(ctx, &args[0].value)?;
                        let (b, _) = emit_expr(ctx, &args[1].value)?;
                        let res_ty = map_ast_type(ctx.b, &ty);
                        let glsl = ctx.get_glsl_ext();
                        // GLSL.std.450: FMin=37, UMin=38, SMin=39
                        let opcode = if is_float_like(&ty) {
                            37
                        } else if is_uint_like(&ty) {
                            38
                        } else {
                            39
                        };
                        let res_id = ctx
                            .b
                            .ext_inst(
                                res_ty,
                                None,
                                glsl,
                                opcode,
                                vec![Operand::IdRef(a), Operand::IdRef(b)],
                            )
                            .unwrap();
                        return Ok((res_id, ty));
                    }
                    "max" if args.len() == 2 => {
                        let (a, ty) = emit_expr(ctx, &args[0].value)?;
                        let (b, _) = emit_expr(ctx, &args[1].value)?;
                        let res_ty = map_ast_type(ctx.b, &ty);
                        let glsl = ctx.get_glsl_ext();
                        // GLSL.std.450: FMax=40, UMax=41, SMax=42
                        let opcode = if is_float_like(&ty) {
                            40
                        } else if is_uint_like(&ty) {
                            41
                        } else {
                            42
                        };
                        let res_id = ctx
                            .b
                            .ext_inst(
                                res_ty,
                                None,
                                glsl,
                                opcode,
                                vec![Operand::IdRef(a), Operand::IdRef(b)],
                            )
                            .unwrap();
                        return Ok((res_id, ty));
                    }
                    "clamp" if args.len() == 3 => {
                        let (val, val_ty) = emit_expr(ctx, &args[0].value)?;
                        let (min_val, min_ty) = emit_expr(ctx, &args[1].value)?;
                        let (max_val, max_ty) = emit_expr(ctx, &args[2].value)?;
                        let (val_f, min_f, max_f, out_ty) = coerce_float_ternary_operands(
                            ctx, val, &val_ty, min_val, &min_ty, max_val, &max_ty,
                        );
                        let res_ty = map_ast_type(ctx.b, &out_ty);
                        let glsl = ctx.get_glsl_ext();
                        let res_id = ctx
                            .b
                            .ext_inst(
                                res_ty,
                                None,
                                glsl,
                                43,
                                vec![
                                    Operand::IdRef(val_f),
                                    Operand::IdRef(min_f),
                                    Operand::IdRef(max_f),
                                ],
                            )
                            .unwrap(); // FClamp = 43
                        return Ok((res_id, out_ty));
                    }
                    "mix" if args.len() == 3 => {
                        let (a, ty) = emit_expr(ctx, &args[0].value)?;
                        let (b, b_ty) = emit_expr(ctx, &args[1].value)?;
                        let (t, t_ty) = emit_expr(ctx, &args[2].value)?;
                        let (a_f, b_f, t_f, out_ty) =
                            coerce_float_ternary_operands(ctx, a, &ty, b, &b_ty, t, &t_ty);
                        let res_ty = map_ast_type(ctx.b, &out_ty);
                        let glsl = ctx.get_glsl_ext();
                        let res_id = ctx
                            .b
                            .ext_inst(
                                res_ty,
                                None,
                                glsl,
                                46,
                                vec![
                                    Operand::IdRef(a_f),
                                    Operand::IdRef(b_f),
                                    Operand::IdRef(t_f),
                                ],
                            )
                            .unwrap(); // FMix = 46
                        return Ok((res_id, out_ty));
                    }
                    "step" if args.len() == 2 => {
                        let (edge, edge_ty) = emit_expr(ctx, &args[0].value)?;
                        let (x, x_ty) = emit_expr(ctx, &args[1].value)?;
                        let (edge_f, x_f, out_ty) =
                            coerce_float_binary_operands(ctx, edge, &edge_ty, x, &x_ty);
                        let res_ty = map_ast_type(ctx.b, &out_ty);
                        let glsl = ctx.get_glsl_ext();
                        let res_id = ctx
                            .b
                            .ext_inst(
                                res_ty,
                                None,
                                glsl,
                                48,
                                vec![Operand::IdRef(edge_f), Operand::IdRef(x_f)],
                            )
                            .unwrap(); // Step = 48
                        return Ok((res_id, out_ty));
                    }
                    "smoothstep" if args.len() == 3 => {
                        let (edge0, edge0_ty) = emit_expr(ctx, &args[0].value)?;
                        let (edge1, edge1_ty) = emit_expr(ctx, &args[1].value)?;
                        let (x, x_ty) = emit_expr(ctx, &args[2].value)?;
                        let (edge0_f, edge1_f, x_f, out_ty) = coerce_float_ternary_operands(
                            ctx, edge0, &edge0_ty, edge1, &edge1_ty, x, &x_ty,
                        );
                        let res_ty = map_ast_type(ctx.b, &out_ty);
                        let glsl = ctx.get_glsl_ext();
                        let res_id = ctx
                            .b
                            .ext_inst(
                                res_ty,
                                None,
                                glsl,
                                49,
                                vec![
                                    Operand::IdRef(edge0_f),
                                    Operand::IdRef(edge1_f),
                                    Operand::IdRef(x_f),
                                ],
                            )
                            .unwrap(); // SmoothStep = 49
                        return Ok((res_id, out_ty));
                    }
                    "length" if args.len() == 1 => {
                        let (val, _) = emit_expr(ctx, &args[0].value)?;
                        let glsl = ctx.get_glsl_ext();
                        let res_id = ctx
                            .b
                            .ext_inst(float, None, glsl, 66, vec![Operand::IdRef(val)])
                            .unwrap(); // Length = 66
                        return Ok((
                            res_id,
                            Type::Named {
                                name: "Float".into(),
                                generics: vec![],
                                span: expr.span(),
                            },
                        ));
                    }
                    "normalize" if args.len() == 1 => {
                        let (val, ty) = emit_expr(ctx, &args[0].value)?;
                        let res_ty = map_ast_type(ctx.b, &ty);
                        let glsl = ctx.get_glsl_ext();
                        let res_id = ctx
                            .b
                            .ext_inst(res_ty, None, glsl, 69, vec![Operand::IdRef(val)])
                            .unwrap(); // Normalize = 69
                        return Ok((res_id, ty));
                    }
                    "dot" if args.len() == 2 => {
                        let (a, _) = emit_expr(ctx, &args[0].value)?;
                        let (b, _) = emit_expr(ctx, &args[1].value)?;
                        let res_id = ctx.b.dot(float, None, a, b).unwrap();
                        return Ok((
                            res_id,
                            Type::Named {
                                name: "Float".into(),
                                generics: vec![],
                                span: expr.span(),
                            },
                        ));
                    }
                    "cross" if args.len() == 2 => {
                        let (a, ty) = emit_expr(ctx, &args[0].value)?;
                        let (b, _) = emit_expr(ctx, &args[1].value)?;
                        let res_ty = map_ast_type(ctx.b, &ty);
                        let glsl = ctx.get_glsl_ext();
                        let res_id = ctx
                            .b
                            .ext_inst(
                                res_ty,
                                None,
                                glsl,
                                68,
                                vec![Operand::IdRef(a), Operand::IdRef(b)],
                            )
                            .unwrap(); // Cross = 68
                        return Ok((res_id, ty));
                    }
                    "reflect" if args.len() == 2 => {
                        let (i, ty) = emit_expr(ctx, &args[0].value)?;
                        let (n, _) = emit_expr(ctx, &args[1].value)?;
                        let res_ty = map_ast_type(ctx.b, &ty);
                        let glsl = ctx.get_glsl_ext();
                        let res_id = ctx
                            .b
                            .ext_inst(
                                res_ty,
                                None,
                                glsl,
                                71,
                                vec![Operand::IdRef(i), Operand::IdRef(n)],
                            )
                            .unwrap(); // Reflect = 71
                        return Ok((res_id, ty));
                    }
                    "asin" if args.len() == 1 => {
                        let (val, ty) = emit_expr(ctx, &args[0].value)?;
                        let res_ty = map_ast_type(ctx.b, &ty);
                        let glsl = ctx.get_glsl_ext();
                        let res_id = ctx
                            .b
                            .ext_inst(res_ty, None, glsl, 16, vec![Operand::IdRef(val)])
                            .unwrap(); // Asin = 16
                        return Ok((res_id, ty));
                    }
                    "acos" if args.len() == 1 => {
                        let (val, ty) = emit_expr(ctx, &args[0].value)?;
                        let res_ty = map_ast_type(ctx.b, &ty);
                        let glsl = ctx.get_glsl_ext();
                        let res_id = ctx
                            .b
                            .ext_inst(res_ty, None, glsl, 17, vec![Operand::IdRef(val)])
                            .unwrap(); // Acos = 17
                        return Ok((res_id, ty));
                    }
                    "atan" if args.len() == 1 => {
                        let (val, ty) = emit_expr(ctx, &args[0].value)?;
                        let res_ty = map_ast_type(ctx.b, &ty);
                        let glsl = ctx.get_glsl_ext();
                        let res_id = ctx
                            .b
                            .ext_inst(res_ty, None, glsl, 18, vec![Operand::IdRef(val)])
                            .unwrap(); // Atan = 18
                        return Ok((res_id, ty));
                    }
                    // atan2(y, x) — GLSL.std.450 opcode 25 (Atan2). Note: opcode 19 = Sinh, not atan2!
                    "atan2" if args.len() == 2 => {
                        let (y, ty) = emit_expr(ctx, &args[0].value)?;
                        let (x, _) = emit_expr(ctx, &args[1].value)?;
                        let res_ty = map_ast_type(ctx.b, &ty);
                        let glsl = ctx.get_glsl_ext();
                        let res_id = ctx
                            .b
                            .ext_inst(
                                res_ty,
                                None,
                                glsl,
                                25,
                                vec![Operand::IdRef(y), Operand::IdRef(x)],
                            )
                            .unwrap(); // Atan2 = 25
                        return Ok((res_id, ty));
                    }
                    // -------------------------------------------------------------------------
                    // Extended math — GLSL.std.450 opcodes verified against spec anchors
                    // -------------------------------------------------------------------------
                    "round" if args.len() == 1 => {
                        let (val, ty) = emit_expr(ctx, &args[0].value)?;
                        let res_ty = map_ast_type(ctx.b, &ty);
                        let glsl = ctx.get_glsl_ext();
                        let res_id = ctx
                            .b
                            .ext_inst(res_ty, None, glsl, 1, vec![Operand::IdRef(val)])
                            .unwrap(); // Round = 1
                        return Ok((res_id, ty));
                    }
                    "trunc" if args.len() == 1 => {
                        let (val, ty) = emit_expr(ctx, &args[0].value)?;
                        let res_ty = map_ast_type(ctx.b, &ty);
                        let glsl = ctx.get_glsl_ext();
                        let res_id = ctx
                            .b
                            .ext_inst(res_ty, None, glsl, 3, vec![Operand::IdRef(val)])
                            .unwrap(); // Trunc = 3
                        return Ok((res_id, ty));
                    }
                    "sign" if args.len() == 1 => {
                        let (val, ty) = emit_expr(ctx, &args[0].value)?;
                        let res_ty = map_ast_type(ctx.b, &ty);
                        let glsl = ctx.get_glsl_ext();
                        let res_id = ctx
                            .b
                            .ext_inst(res_ty, None, glsl, 6, vec![Operand::IdRef(val)])
                            .unwrap(); // FSign = 6
                        return Ok((res_id, ty));
                    }
                    "radians" if args.len() == 1 => {
                        let (val, ty) = emit_expr(ctx, &args[0].value)?;
                        let res_ty = map_ast_type(ctx.b, &ty);
                        let glsl = ctx.get_glsl_ext();
                        let res_id = ctx
                            .b
                            .ext_inst(res_ty, None, glsl, 11, vec![Operand::IdRef(val)])
                            .unwrap(); // Radians = 11
                        return Ok((res_id, ty));
                    }
                    "degrees" if args.len() == 1 => {
                        let (val, ty) = emit_expr(ctx, &args[0].value)?;
                        let res_ty = map_ast_type(ctx.b, &ty);
                        let glsl = ctx.get_glsl_ext();
                        let res_id = ctx
                            .b
                            .ext_inst(res_ty, None, glsl, 12, vec![Operand::IdRef(val)])
                            .unwrap(); // Degrees = 12
                        return Ok((res_id, ty));
                    }
                    "exp" if args.len() == 1 => {
                        let (val, ty) = emit_expr(ctx, &args[0].value)?;
                        let res_ty = map_ast_type(ctx.b, &ty);
                        let glsl = ctx.get_glsl_ext();
                        let res_id = ctx
                            .b
                            .ext_inst(res_ty, None, glsl, 27, vec![Operand::IdRef(val)])
                            .unwrap(); // Exp = 27
                        return Ok((res_id, ty));
                    }
                    "log" if args.len() == 1 => {
                        let (val, ty) = emit_expr(ctx, &args[0].value)?;
                        let res_ty = map_ast_type(ctx.b, &ty);
                        let glsl = ctx.get_glsl_ext();
                        let res_id = ctx
                            .b
                            .ext_inst(res_ty, None, glsl, 28, vec![Operand::IdRef(val)])
                            .unwrap(); // Log = 28
                        return Ok((res_id, ty));
                    }
                    "exp2" if args.len() == 1 => {
                        let (val, ty) = emit_expr(ctx, &args[0].value)?;
                        let res_ty = map_ast_type(ctx.b, &ty);
                        let glsl = ctx.get_glsl_ext();
                        let res_id = ctx
                            .b
                            .ext_inst(res_ty, None, glsl, 29, vec![Operand::IdRef(val)])
                            .unwrap(); // Exp2 = 29
                        return Ok((res_id, ty));
                    }
                    "log2" if args.len() == 1 => {
                        let (val, ty) = emit_expr(ctx, &args[0].value)?;
                        let res_ty = map_ast_type(ctx.b, &ty);
                        let glsl = ctx.get_glsl_ext();
                        let res_id = ctx
                            .b
                            .ext_inst(res_ty, None, glsl, 30, vec![Operand::IdRef(val)])
                            .unwrap(); // Log2 = 30
                        return Ok((res_id, ty));
                    }
                    "inversesqrt" if args.len() == 1 => {
                        let (val, ty) = emit_expr(ctx, &args[0].value)?;
                        let res_ty = map_ast_type(ctx.b, &ty);
                        let glsl = ctx.get_glsl_ext();
                        let res_id = ctx
                            .b
                            .ext_inst(res_ty, None, glsl, 32, vec![Operand::IdRef(val)])
                            .unwrap(); // InverseSqrt = 32
                        return Ok((res_id, ty));
                    }
                    // mod(x, y) = x - y * floor(x/y) — uses core OpFMod (not ext_inst)
                    "mod" if args.len() == 2 => {
                        let (x, ty) = emit_expr(ctx, &args[0].value)?;
                        let (y, _) = emit_expr(ctx, &args[1].value)?;
                        let res_ty = map_ast_type(ctx.b, &ty);
                        let res_id = ctx.b.f_mod(res_ty, None, x, y).unwrap();
                        return Ok((res_id, ty));
                    }
                    // distance(a, b) = length(a - b) — returns Float scalar
                    "distance" if args.len() == 2 => {
                        let (a, _) = emit_expr(ctx, &args[0].value)?;
                        let (b, _) = emit_expr(ctx, &args[1].value)?;
                        let glsl = ctx.get_glsl_ext();
                        let res_id = ctx
                            .b
                            .ext_inst(
                                float,
                                None,
                                glsl,
                                67,
                                vec![Operand::IdRef(a), Operand::IdRef(b)],
                            )
                            .unwrap(); // Distance = 67
                        return Ok((
                            res_id,
                            Type::Named {
                                name: "Float".into(),
                                generics: vec![],
                                span: expr.span(),
                            },
                        ));
                    }
                    // refract(incident, normal, eta) — eta is ratio of indices of refraction
                    "refract" if args.len() == 3 => {
                        let (i, ty) = emit_expr(ctx, &args[0].value)?;
                        let (n, _) = emit_expr(ctx, &args[1].value)?;
                        let (eta, _) = emit_expr(ctx, &args[2].value)?;
                        let res_ty = map_ast_type(ctx.b, &ty);
                        let glsl = ctx.get_glsl_ext();
                        let res_id = ctx
                            .b
                            .ext_inst(
                                res_ty,
                                None,
                                glsl,
                                72,
                                vec![Operand::IdRef(i), Operand::IdRef(n), Operand::IdRef(eta)],
                            )
                            .unwrap(); // Refract = 72
                        return Ok((res_id, ty));
                    }

                    // Texture sampling
                    "sample" if args.len() == 2 => {
                        let (sampler, _) = emit_expr(ctx, &args[0].value)?;
                        let (coords, _) = emit_expr(ctx, &args[1].value)?;
                        let vec4 = ctx.b.type_vector(float, 4);
                        let res_id = ctx
                            .b
                            .image_sample_implicit_lod(
                                vec4,
                                None,
                                sampler,
                                coords,
                                None,
                                std::iter::empty(),
                            )
                            .unwrap();
                        return Ok((
                            res_id,
                            Type::Named {
                                name: "Vec4".into(),
                                generics: vec![],
                                span: expr.span(),
                            },
                        ));
                    }
                    "sample_lod" if args.len() == 3 => {
                        let (sampler, _) = emit_expr(ctx, &args[0].value)?;
                        let (coords, _) = emit_expr(ctx, &args[1].value)?;
                        let (lod, _) = emit_expr(ctx, &args[2].value)?;
                        let vec4 = ctx.b.type_vector(float, 4);
                        let res_id = ctx
                            .b
                            .image_sample_explicit_lod(
                                vec4,
                                None,
                                sampler,
                                coords,
                                rspirv::spirv::ImageOperands::LOD,
                                vec![Operand::IdRef(lod)],
                            )
                            .unwrap();
                        return Ok((
                            res_id,
                            Type::Named {
                                name: "Vec4".into(),
                                generics: vec![],
                                span: expr.span(),
                            },
                        ));
                    }

                    _ => {}
                }
            }
            let callee_name = match &**callee {
                Expr::Ident(name, _) => name.clone(),
                _ => "<complex expression>".to_string(),
            };
            Err(KainError::codegen(
                format!("Unsupported function call in shader: '{}'", callee_name),
                expr.span(),
            ))
        }
        Expr::Float(f, span) => {
            let float = ctx.b.type_float(32);
            let val = ctx.b.constant_bit32(float, (*f as f32).to_bits());
            Ok((
                val,
                Type::Named {
                    name: "Float".into(),
                    generics: vec![],
                    span: *span,
                },
            ))
        }
        Expr::Int(i, span) => {
            let int = ctx.b.type_int(32, 1);
            let val = ctx.b.constant_bit32(int, *i as u32);
            Ok((
                val,
                Type::Named {
                    name: "Int".into(),
                    generics: vec![],
                    span: *span,
                },
            ))
        }
        Expr::Bool(v, span) => {
            let bool_ty = ctx.b.type_bool();
            let bool_id = if *v {
                ctx.b.constant_true(bool_ty)
            } else {
                ctx.b.constant_false(bool_ty)
            };
            Ok((
                bool_id,
                Type::Named {
                    name: "Bool".into(),
                    generics: vec![],
                    span: *span,
                },
            ))
        }
        Expr::Paren(inner, _) => emit_expr(ctx, inner),
        Expr::Cast {
            value,
            target,
            span,
        } => {
            let (src_val, src_ty) = emit_expr(ctx, value)?;
            let dst_ty = target.clone();
            let dst_ty_id = map_ast_type(ctx.b, &dst_ty);

            if is_same_type_family(&src_ty, &dst_ty) {
                return Ok((src_val, dst_ty));
            }

            if is_bool(&dst_ty) {
                let as_b = as_bool(ctx, src_val, &src_ty);
                return Ok((as_b, dst_ty));
            }

            if is_bool(&src_ty) {
                let src_b = as_bool(ctx, src_val, &src_ty);
                let out = if is_float(&dst_ty) || is_float_vector(&dst_ty) {
                    let one = float_one(ctx.b, &dst_ty);
                    let zero = float_zero(ctx.b, &dst_ty);
                    ctx.b.select(dst_ty_id, None, src_b, one, zero).unwrap()
                } else if is_uint_like(&dst_ty) {
                    let one = uint_one(ctx.b, &dst_ty);
                    let zero = uint_zero(ctx.b, &dst_ty);
                    ctx.b.select(dst_ty_id, None, src_b, one, zero).unwrap()
                } else if is_int_like(&dst_ty) {
                    let one = int_one(ctx.b, &dst_ty);
                    let zero = int_zero(ctx.b, &dst_ty);
                    ctx.b.select(dst_ty_id, None, src_b, one, zero).unwrap()
                } else {
                    return Err(KainError::codegen(
                        "Unsupported bool cast target in shader",
                        *span,
                    ));
                };
                return Ok((out, dst_ty));
            }

            let src_dim = numeric_dim(&src_ty);
            let dst_dim = numeric_dim(&dst_ty);
            if src_dim == 0 || dst_dim == 0 || src_dim != dst_dim {
                return Err(KainError::codegen(
                    "Invalid cast in shader: source/target dimensions or categories are incompatible",
                    *span,
                ));
            }

            let converted = if is_float_like(&src_ty) && is_int_like(&dst_ty) {
                ctx.b.convert_f_to_s(dst_ty_id, None, src_val).unwrap()
            } else if is_float_like(&src_ty) && is_uint_like(&dst_ty) {
                ctx.b.convert_f_to_u(dst_ty_id, None, src_val).unwrap()
            } else if is_int_like(&src_ty) && is_float_like(&dst_ty) {
                ctx.b.convert_s_to_f(dst_ty_id, None, src_val).unwrap()
            } else if is_uint_like(&src_ty) && is_float_like(&dst_ty) {
                ctx.b.convert_u_to_f(dst_ty_id, None, src_val).unwrap()
            } else if is_int_like(&src_ty) && is_uint_like(&dst_ty) {
                ctx.b.bitcast(dst_ty_id, None, src_val).unwrap()
            } else if is_uint_like(&src_ty) && is_int_like(&dst_ty) {
                ctx.b.bitcast(dst_ty_id, None, src_val).unwrap()
            } else if is_int_like(&src_ty) && is_int_like(&dst_ty) {
                ctx.b.s_convert(dst_ty_id, None, src_val).unwrap()
            } else if is_uint_like(&src_ty) && is_uint_like(&dst_ty) {
                ctx.b.u_convert(dst_ty_id, None, src_val).unwrap()
            } else {
                return Err(KainError::codegen(
                    "Unsupported numeric cast in shader",
                    *span,
                ));
            };
            Ok((converted, dst_ty))
        }
        Expr::Assign { target, value, .. } => {
            let (next_val, next_ty) = emit_expr(ctx, value)?;
            match target.as_ref() {
                Expr::Ident(name, span) => {
                    if let Some(binding) = ctx.vars.get(name).cloned() {
                        if !binding.is_ptr {
                            return Err(KainError::codegen(
                                format!("Cannot assign to immutable value '{}'", name),
                                *span,
                            ));
                        }
                        if is_storage_buffer(&binding.ty) {
                            return Err(KainError::codegen(
                                format!(
                                    "Assigning a whole storage buffer '{}' is not supported",
                                    name
                                ),
                                *span,
                            ));
                        }
                        ctx.b
                            .store(binding.id, next_val, None, std::iter::empty())
                            .unwrap();
                        Ok((next_val, next_ty))
                    } else {
                        Err(KainError::codegen(
                            format!("Unknown assignment target: {}", name),
                            *span,
                        ))
                    }
                }
                Expr::Index { object, index, .. } => {
                    let elem_ptr =
                        emit_index_lvalue_ptr(ctx, object, index, &next_ty, target.span())?;
                    ctx.b
                        .store(elem_ptr, next_val, None, std::iter::empty())
                        .unwrap();
                    Ok((next_val, next_ty))
                }
                _ => Err(KainError::codegen(
                    "Unsupported assignment target in shader",
                    target.span(),
                )),
            }
        }
        Expr::Index { object, index, .. } => {
            // StorageBuffer<T> indexing: emit AccessChain to runtime array element.
            if let Expr::Ident(buffer_name, span) = object.as_ref() {
                if ctx.storage_buffers.contains(buffer_name) {
                    let binding = ctx.vars.get(buffer_name).cloned().ok_or_else(|| {
                        KainError::codegen(
                            format!("Unknown storage buffer: {}", buffer_name),
                            *span,
                        )
                    })?;
                    let (index_raw_id, index_raw_ty) = emit_expr(ctx, index)?;
                    let index_id =
                        coerce_to_u32_index(ctx, index_raw_id, &index_raw_ty, expr.span())?;
                    let elem_ty = storage_buffer_elem_type(&binding.ty, expr.span());
                    let elem_ty_id = map_ast_type(ctx.b, &elem_ty);
                    let elem_ptr_ty =
                        ctx.b
                            .type_pointer(None, StorageClass::StorageBuffer, elem_ty_id);
                    let uint_ty = ctx.b.type_int(32, 0);
                    let zero = ctx.b.constant_bit32(uint_ty, 0);
                    let elem_ptr = ctx
                        .b
                        .access_chain(elem_ptr_ty, None, binding.id, vec![zero, index_id])
                        .unwrap();
                    let loaded = ctx
                        .b
                        .load(elem_ty_id, None, elem_ptr, None, std::iter::empty())
                        .unwrap();
                    return Ok((loaded, elem_ty));
                }
            }
            Err(KainError::codegen(
                "Unsupported index expression in shader",
                expr.span(),
            ))
        }
        Expr::Field {
            object,
            field,
            span,
        } => {
            let (obj_id, obj_ty) = emit_expr(ctx, object)?;
            if let Some(indices) = swizzle_indices(field) {
                let scalar_ty = vector_scalar_type(ctx, &obj_ty);
                let scalar_spv = map_ast_type(ctx.b, &scalar_ty);
                if indices.len() == 1 {
                    let res_id = ctx
                        .b
                        .composite_extract(scalar_spv, None, obj_id, vec![indices[0]])
                        .unwrap();
                    return Ok((res_id, scalar_ty));
                }
                let swizzle_len = indices.len();
                let out_vec = ctx.b.type_vector(scalar_spv, swizzle_len as u32);
                let res_id = ctx
                    .b
                    .vector_shuffle(out_vec, None, obj_id, obj_id, indices)
                    .unwrap();
                return Ok((res_id, vec_type_from_scalar(&scalar_ty, swizzle_len, *span)));
            }
            Err(KainError::codegen(
                format!("Unsupported field access: {}", field),
                *span,
            ))
        }
        Expr::Unary { op, operand, .. } => {
            let (val, ty) = emit_expr(ctx, operand)?;
            let ty_id = map_ast_type(ctx.b, &ty);
            let out = match op {
                UnaryOp::Neg => {
                    if is_float_like(&ty) {
                        ctx.b.f_negate(ty_id, None, val).unwrap()
                    } else {
                        ctx.b.s_negate(ty_id, None, val).unwrap()
                    }
                }
                UnaryOp::Not => {
                    let bool_ty = ctx.b.type_bool();
                    let b = as_bool(ctx, val, &ty);
                    return Ok((
                        ctx.b.logical_not(bool_ty, None, b).unwrap(),
                        Type::Named {
                            name: "Bool".into(),
                            generics: vec![],
                            span: expr.span(),
                        },
                    ));
                }
                UnaryOp::BitNot => ctx.b.not(ty_id, None, val).unwrap(),
                UnaryOp::Ref | UnaryOp::RefMut | UnaryOp::Deref => val,
            };
            Ok((out, ty))
        }
        Expr::MethodCall {
            receiver,
            method,
            args,
            span,
        } => {
            let (recv_val, recv_ty) = emit_expr(ctx, receiver)?;
            let float = ctx.b.type_float(32);
            match method.as_str() {
                "length" if args.is_empty() => {
                    let glsl = ctx.get_glsl_ext();
                    let id = ctx
                        .b
                        .ext_inst(float, None, glsl, 66, vec![Operand::IdRef(recv_val)])
                        .unwrap();
                    Ok((
                        id,
                        Type::Named {
                            name: "Float".into(),
                            generics: vec![],
                            span: *span,
                        },
                    ))
                }
                "normalize" if args.is_empty() => {
                    let res_ty = map_ast_type(ctx.b, &recv_ty);
                    let glsl = ctx.get_glsl_ext();
                    let id = ctx
                        .b
                        .ext_inst(res_ty, None, glsl, 69, vec![Operand::IdRef(recv_val)])
                        .unwrap();
                    Ok((id, recv_ty))
                }
                "dot" if args.len() == 1 => {
                    let (rhs, _) = emit_expr(ctx, &args[0].value)?;
                    let id = ctx.b.dot(float, None, recv_val, rhs).unwrap();
                    Ok((
                        id,
                        Type::Named {
                            name: "Float".into(),
                            generics: vec![],
                            span: *span,
                        },
                    ))
                }
                "cross" if args.len() == 1 => {
                    let (rhs, _) = emit_expr(ctx, &args[0].value)?;
                    let res_ty = map_ast_type(ctx.b, &recv_ty);
                    let glsl = ctx.get_glsl_ext();
                    let id = ctx
                        .b
                        .ext_inst(
                            res_ty,
                            None,
                            glsl,
                            68,
                            vec![Operand::IdRef(recv_val), Operand::IdRef(rhs)],
                        )
                        .unwrap();
                    Ok((id, recv_ty))
                }
                "reflect" if args.len() == 1 => {
                    let (rhs, _) = emit_expr(ctx, &args[0].value)?;
                    let res_ty = map_ast_type(ctx.b, &recv_ty);
                    let glsl = ctx.get_glsl_ext();
                    let id = ctx
                        .b
                        .ext_inst(
                            res_ty,
                            None,
                            glsl,
                            71,
                            vec![Operand::IdRef(recv_val), Operand::IdRef(rhs)],
                        )
                        .unwrap();
                    Ok((id, recv_ty))
                }
                _ => Err(KainError::codegen(
                    format!("Unsupported method call in shader: {}", method),
                    *span,
                )),
            }
        }
        Expr::If {
            condition,
            then_branch,
            else_branch,
            span,
        } => {
            let (cond_raw, cond_ty) = emit_expr(ctx, condition)?;
            let cond = as_bool(ctx, cond_raw, &cond_ty);

            let then_expr = single_expr_from_block(then_branch, *span)?;
            let (then_val, then_ty) = emit_expr(ctx, then_expr)?;

            let else_expr = match else_branch.as_deref() {
                Some(ElseBranch::Else(block)) => single_expr_from_block(block, *span)?,
                Some(ElseBranch::ElseIf(_, _, _)) => {
                    return Err(KainError::codegen(
                        "Else-if expression chains are not yet supported in SPIR-V value expressions",
                        *span,
                    ));
                }
                None => {
                    return Err(KainError::codegen(
                        "If expression requires an else branch in SPIR-V backend",
                        *span,
                    ));
                }
            };
            let (else_val, else_ty) = emit_expr(ctx, else_expr)?;
            if std::mem::discriminant(&then_ty) != std::mem::discriminant(&else_ty) {
                return Err(KainError::codegen(
                    "If expression branches must return compatible types",
                    *span,
                ));
            }
            let res_ty = map_ast_type(ctx.b, &then_ty);
            let selected = ctx
                .b
                .select(res_ty, None, cond, then_val, else_val)
                .unwrap();
            Ok((selected, then_ty))
        }
        _ => Err(KainError::codegen(
            format!("Unsupported expression in shader: {}", expr_kind_name(expr)),
            expr.span(),
        )),
    }
}

fn expr_kind_name(expr: &Expr) -> &'static str {
    match expr {
        Expr::Int(..) => "Int",
        Expr::Float(..) => "Float",
        Expr::Bool(..) => "Bool",
        Expr::None(..) => "None",
        Expr::String(..) => "String",
        Expr::FString(..) => "FString",
        Expr::Ident(..) => "Ident",
        Expr::MacroCall { .. } => "MacroCall",
        Expr::Binary { .. } => "Binary",
        Expr::Unary { .. } => "Unary",
        Expr::Call { .. } => "Call",
        Expr::MethodCall { .. } => "MethodCall",
        Expr::Field { .. } => "Field",
        Expr::Index { .. } => "Index",
        Expr::Assign { .. } => "Assign",
        Expr::Struct { .. } => "Struct",
        Expr::AggregateInit { .. } => "AggregateInit",
        Expr::EnumVariant { .. } => "EnumVariant",
        Expr::Tuple(..) => "Tuple",
        Expr::Array(..) => "Array",
        Expr::Range { .. } => "Range",
        Expr::If { .. } => "If",
        Expr::Match { .. } => "Match",
        Expr::Lambda { .. } => "Lambda",
        Expr::Ref { .. } => "Ref",
        Expr::AddrOf { .. } => "AddrOf",
        Expr::Deref(..) => "Deref",
        Expr::PtrOffset { .. } => "PtrOffset",
        Expr::MemLoad { .. } => "MemLoad",
        Expr::MemStore { .. } => "MemStore",
        Expr::SizeOfType { .. } => "SizeOfType",
        Expr::AlignOfType { .. } => "AlignOfType",
        Expr::Alloca { .. } => "Alloca",
        Expr::Uninit { .. } => "Uninit",
        Expr::Alloc { .. } => "Alloc",
        Expr::Realloc { .. } => "Realloc",
        Expr::Cast { .. } => "Cast",
        Expr::Try(..) => "Try",
        Expr::Await(..) => "Await",
        Expr::Spawn { .. } => "Spawn",
        Expr::SendMsg { .. } => "SendMsg",
        Expr::Comptime(..) => "Comptime",
        Expr::Block(..) => "Block",
        Expr::JSX(..) => "JSX",
        Expr::Paren(..) => "Paren",
        Expr::Return(..) => "Return",
        Expr::Break(..) => "Break",
        Expr::Continue(..) => "Continue",
    }
}

fn single_expr_from_block<'a>(
    block: &'a Block,
    span: kain_core::span::Span,
) -> KainResult<&'a Expr> {
    if block.stmts.len() != 1 {
        return Err(KainError::codegen("Expected single-expression block", span));
    }
    match &block.stmts[0] {
        Stmt::Expr(expr) => Ok(expr),
        _ => Err(KainError::codegen(
            "Expected expression statement in block",
            span,
        )),
    }
}

fn as_bool(ctx: &mut ShaderContext, value: u32, ty: &Type) -> u32 {
    if matches!(ty, Type::Named { name, .. } if name == "Bool") {
        return value;
    }
    let bool_ty = ctx.b.type_bool();
    if is_uint(ty) {
        let uint_ty = map_ast_type(ctx.b, ty);
        let zero = ctx.b.constant_bit32(uint_ty, 0);
        return ctx.b.i_not_equal(bool_ty, None, value, zero).unwrap();
    }
    if is_int(ty) {
        let int_ty = map_ast_type(ctx.b, ty);
        let zero = ctx.b.constant_bit32(int_ty, 0);
        return ctx.b.i_not_equal(bool_ty, None, value, zero).unwrap();
    }
    let float_ty = ctx.b.type_float(32);
    let zero = ctx.b.constant_bit32(float_ty, 0f32.to_bits());
    ctx.b.f_ord_not_equal(bool_ty, None, value, zero).unwrap()
}

fn terminate_with_branch(ctx: &mut ShaderContext, target: u32) -> KainResult<()> {
    if ctx.b.selected_block().is_some() {
        ctx.b.branch(target).unwrap();
    }
    Ok(())
}

fn emit_if_statement(
    ctx: &mut ShaderContext,
    condition: &Expr,
    then_branch: &Block,
    else_branch: Option<&ElseBranch>,
) -> KainResult<()> {
    let (cond_raw, cond_ty) = emit_expr(ctx, condition)?;
    let cond = as_bool(ctx, cond_raw, &cond_ty);

    let then_label = ctx.b.id();
    let else_label = ctx.b.id();
    let merge_label = ctx.b.id();
    ctx.b
        .selection_merge(merge_label, rspirv::spirv::SelectionControl::NONE)
        .unwrap();
    ctx.b
        .branch_conditional(cond, then_label, else_label, std::iter::empty::<u32>())
        .unwrap();

    ctx.b.begin_block(Some(then_label)).unwrap();
    emit_block(ctx, then_branch)?;
    terminate_with_branch(ctx, merge_label)?;

    ctx.b.begin_block(Some(else_label)).unwrap();
    if let Some(else_branch) = else_branch {
        emit_else_branch(ctx, else_branch)?;
    }
    terminate_with_branch(ctx, merge_label)?;

    ctx.b.begin_block(Some(merge_label)).unwrap();
    Ok(())
}

fn emit_else_branch(ctx: &mut ShaderContext, else_branch: &ElseBranch) -> KainResult<()> {
    match else_branch {
        ElseBranch::Else(block) => emit_block(ctx, block),
        ElseBranch::ElseIf(cond, then_block, nested) => {
            emit_if_statement(ctx, cond, then_block, nested.as_deref())
        }
    }
}

fn emit_while_statement(ctx: &mut ShaderContext, condition: &Expr, body: &Block) -> KainResult<()> {
    let header_label = ctx.b.id();
    let condition_label = ctx.b.id();
    let loop_body_label = ctx.b.id();
    let continue_label = ctx.b.id();
    let merge_label = ctx.b.id();

    terminate_with_branch(ctx, header_label)?;

    ctx.b.begin_block(Some(header_label)).unwrap();
    ctx.b
        .loop_merge(
            merge_label,
            continue_label,
            rspirv::spirv::LoopControl::NONE,
            std::iter::empty(),
        )
        .unwrap();
    ctx.b.branch(condition_label).unwrap();

    ctx.b.begin_block(Some(condition_label)).unwrap();
    let (cond_raw, cond_ty) = emit_expr(ctx, condition)?;
    let cond = as_bool(ctx, cond_raw, &cond_ty);
    ctx.b
        .branch_conditional(
            cond,
            loop_body_label,
            merge_label,
            std::iter::empty::<u32>(),
        )
        .unwrap();

    ctx.loop_continue_targets.push(continue_label);
    ctx.loop_break_targets.push(merge_label);

    ctx.b.begin_block(Some(loop_body_label)).unwrap();
    emit_block(ctx, body)?;
    terminate_with_branch(ctx, continue_label)?;

    ctx.loop_continue_targets.pop();
    ctx.loop_break_targets.pop();

    ctx.b.begin_block(Some(continue_label)).unwrap();
    terminate_with_branch(ctx, header_label)?;

    ctx.b.begin_block(Some(merge_label)).unwrap();
    Ok(())
}

fn emit_for_statement(
    ctx: &mut ShaderContext,
    binding: &Pattern,
    iter: &Expr,
    body: &Block,
    span: kain_core::span::Span,
) -> KainResult<()> {
    let (name, bind_span) = match binding {
        Pattern::Binding { name, span, .. } => (name.clone(), *span),
        _ => {
            return Err(KainError::codegen(
                "for loop requires binding pattern",
                span,
            ))
        }
    };

    let (start_expr, end_expr, inclusive) = match iter {
        Expr::Range {
            start,
            end,
            inclusive,
            ..
        } => (start.as_deref(), end.as_deref(), *inclusive),
        Expr::Call { callee, args, .. } => {
            if let Expr::Ident(fn_name, _) = callee.as_ref() {
                if fn_name == "range" {
                    match args.len() {
                        1 => (None, Some(&args[0].value), false),
                        2 => (Some(&args[0].value), Some(&args[1].value), false),
                        _ => {
                            return Err(KainError::codegen(
                                "SPIR-V for loops support range(end) or range(start, end)",
                                span,
                            ))
                        }
                    }
                } else {
                    return Err(KainError::codegen(
                        "SPIR-V for loops currently require range iteration",
                        span,
                    ));
                }
            } else {
                return Err(KainError::codegen(
                    "SPIR-V for loops currently require range iteration",
                    span,
                ));
            }
        }
        _ => {
            return Err(KainError::codegen(
                "SPIR-V for loops currently require range iteration",
                span,
            ))
        }
    };
    let end_expr =
        end_expr.ok_or_else(|| KainError::codegen("for range requires an end bound", span))?;

    let int_ty_ast = Type::Named {
        name: "Int".into(),
        generics: vec![],
        span: bind_span,
    };
    let int_ty = map_ast_type(ctx.b, &int_ty_ast);
    let int_ptr_ty = ctx.b.type_pointer(None, StorageClass::Function, int_ty);

    let loop_var_name = internal_hoist_name("for_iter", span);
    let loop_var = take_hoisted_slot(ctx, &loop_var_name, int_ptr_ty).unwrap_or_else(|| {
        ctx.b
            .variable(int_ptr_ty, None, StorageClass::Function, None)
    });
    let init_val = if let Some(start_expr) = start_expr {
        let (v, _) = emit_expr(ctx, start_expr)?;
        v
    } else {
        ctx.b.constant_bit32(int_ty, 0)
    };
    ctx.b
        .store(loop_var, init_val, None, std::iter::empty())
        .unwrap();

    let range_end_name = internal_hoist_name("for_end", span);
    let range_end_var = take_hoisted_slot(ctx, &range_end_name, int_ptr_ty).unwrap_or_else(|| {
        ctx.b
            .variable(int_ptr_ty, None, StorageClass::Function, None)
    });
    let (end_val, _) = emit_expr(ctx, end_expr)?;
    ctx.b
        .store(range_end_var, end_val, None, std::iter::empty())
        .unwrap();

    let previous = ctx.vars.insert(
        name.clone(),
        VarBinding {
            id: loop_var,
            ty: int_ty_ast.clone(),
            is_ptr: true,
        },
    );

    let header_label = ctx.b.id();
    let condition_label = ctx.b.id();
    let loop_body_label = ctx.b.id();
    let continue_label = ctx.b.id();
    let merge_label = ctx.b.id();

    terminate_with_branch(ctx, header_label)?;
    ctx.b.begin_block(Some(header_label)).unwrap();
    ctx.b
        .loop_merge(
            merge_label,
            continue_label,
            rspirv::spirv::LoopControl::NONE,
            std::iter::empty(),
        )
        .unwrap();
    ctx.b.branch(condition_label).unwrap();

    ctx.b.begin_block(Some(condition_label)).unwrap();
    let i_val = ctx
        .b
        .load(int_ty, None, loop_var, None, std::iter::empty())
        .unwrap();
    let end_loaded = ctx
        .b
        .load(int_ty, None, range_end_var, None, std::iter::empty())
        .unwrap();
    let bool_ty = ctx.b.type_bool();
    let cond = if inclusive {
        ctx.b
            .s_less_than_equal(bool_ty, None, i_val, end_loaded)
            .unwrap()
    } else {
        ctx.b.s_less_than(bool_ty, None, i_val, end_loaded).unwrap()
    };
    ctx.b
        .branch_conditional(
            cond,
            loop_body_label,
            merge_label,
            std::iter::empty::<u32>(),
        )
        .unwrap();

    ctx.loop_continue_targets.push(continue_label);
    ctx.loop_break_targets.push(merge_label);

    ctx.b.begin_block(Some(loop_body_label)).unwrap();
    emit_block(ctx, body)?;
    terminate_with_branch(ctx, continue_label)?;

    ctx.loop_continue_targets.pop();
    ctx.loop_break_targets.pop();

    ctx.b.begin_block(Some(continue_label)).unwrap();
    let cur_i = ctx
        .b
        .load(int_ty, None, loop_var, None, std::iter::empty())
        .unwrap();
    let one = ctx.b.constant_bit32(int_ty, 1);
    let next_i = ctx.b.i_add(int_ty, None, cur_i, one).unwrap();
    ctx.b
        .store(loop_var, next_i, None, std::iter::empty())
        .unwrap();
    terminate_with_branch(ctx, header_label)?;

    ctx.b.begin_block(Some(merge_label)).unwrap();
    if let Some(prev) = previous {
        ctx.vars.insert(name, prev);
    } else {
        ctx.vars.remove(&name);
    }
    Ok(())
}

fn map_ast_type(b: &mut Builder, ty: &Type) -> u32 {
    let float = b.type_float(32);
    let int = b.type_int(32, 1);
    let uint = b.type_int(32, 0);
    match ty {
        Type::Named { name, generics, .. } => match name.as_str() {
            "Float" | "f32" => float,
            "Int" | "i32" => int,
            "UInt" | "u32" => uint,
            "Bool" => b.type_bool(),
            "Vec2" => b.type_vector(float, 2),
            "Vec3" => b.type_vector(float, 3),
            "Vec4" => b.type_vector(float, 4),
            "IVec2" => b.type_vector(int, 2),
            "IVec3" => b.type_vector(int, 3),
            "IVec4" => b.type_vector(int, 4),
            "UVec2" => b.type_vector(uint, 2),
            "UVec3" => b.type_vector(uint, 3),
            "UVec4" => b.type_vector(uint, 4),
            "Mat4" => {
                let v4 = b.type_vector(float, 4);
                b.type_matrix(v4, 4)
            }
            "Sampler2D" => {
                // Dim2D, NotDepth, Arrayed=False, MS=False, Sampled=1, Format=Unknown
                let image = b.type_image(
                    float,
                    rspirv::spirv::Dim::Dim2D,
                    0,
                    0,
                    0,
                    1,
                    rspirv::spirv::ImageFormat::Unknown,
                    None,
                );
                b.type_sampled_image(image)
            }
            "StorageBuffer" => {
                // StorageBuffer<T> lowers to Block-wrapped runtime array of T.
                let elem_type_ast = if let Some(first) = generics.first() {
                    first.clone()
                } else {
                    Type::Named {
                        name: "Float".into(),
                        generics: vec![],
                        span: kain_core::span::Span::default(),
                    }
                };
                let elem_ty = map_ast_type(b, &elem_type_ast);
                let rt_array = b.type_runtime_array(elem_ty);
                let struct_ty = b.type_struct(vec![rt_array]);
                struct_ty
            }
            "Void" => b.type_void(),
            _ => b.type_void(),
        },
        Type::Ref { inner, .. } => map_ast_type(b, inner),
        Type::Array(inner, _, _) | Type::Slice(inner, _) => map_ast_type(b, inner),
        _ => b.type_void(),
    }
}

fn is_void(ty: &Type) -> bool {
    matches!(ty, Type::Named { name, .. } if name == "Void")
}

fn is_vec4(ty: &Type) -> bool {
    matches!(ty, Type::Named { name, .. } if name == "Vec4")
}

fn is_mat4(ty: &Type) -> bool {
    matches!(ty, Type::Named { name, .. } if name == "Mat4")
}

fn is_float(ty: &Type) -> bool {
    matches!(ty, Type::Named { name, .. } if name == "Float" || name == "f32")
}

fn is_bool(ty: &Type) -> bool {
    matches!(ty, Type::Named { name, .. } if name == "Bool")
}

fn is_uint(ty: &Type) -> bool {
    matches!(ty, Type::Named { name, .. } if name == "UInt" || name == "u32")
}

fn is_int(ty: &Type) -> bool {
    matches!(ty, Type::Named { name, .. } if name == "Int" || name == "i32")
}

fn type_name(ty: &Type) -> Option<&str> {
    if let Type::Named { name, .. } = ty {
        Some(name.as_str())
    } else {
        None
    }
}

fn numeric_dim(ty: &Type) -> usize {
    match type_name(ty) {
        Some("Float") | Some("f32") | Some("Int") | Some("i32") | Some("UInt") | Some("u32")
        | Some("Bool") => 1,
        Some("Vec2") | Some("IVec2") | Some("UVec2") => 2,
        Some("Vec3") | Some("IVec3") | Some("UVec3") => 3,
        Some("Vec4") | Some("IVec4") | Some("UVec4") => 4,
        _ => 0,
    }
}

fn is_float_vector(ty: &Type) -> bool {
    matches!(type_name(ty), Some("Vec2" | "Vec3" | "Vec4"))
}

fn is_int_vector(ty: &Type) -> bool {
    matches!(type_name(ty), Some("IVec2" | "IVec3" | "IVec4"))
}

fn is_uint_vector(ty: &Type) -> bool {
    matches!(type_name(ty), Some("UVec2" | "UVec3" | "UVec4"))
}

fn is_float_like(ty: &Type) -> bool {
    is_float(ty) || is_float_vector(ty)
}

fn is_int_like(ty: &Type) -> bool {
    is_int(ty) || is_int_vector(ty)
}

fn is_uint_like(ty: &Type) -> bool {
    is_uint(ty) || is_uint_vector(ty)
}

fn is_same_type_family(a: &Type, b: &Type) -> bool {
    type_name(a) == type_name(b)
}

fn float_zero(b: &mut Builder, ty: &Type) -> u32 {
    let float = b.type_float(32);
    match numeric_dim(ty) {
        2 => {
            let vec = b.type_vector(float, 2);
            let z = b.constant_bit32(float, 0.0f32.to_bits());
            b.constant_composite(vec, vec![z, z])
        }
        3 => {
            let vec = b.type_vector(float, 3);
            let z = b.constant_bit32(float, 0.0f32.to_bits());
            b.constant_composite(vec, vec![z, z, z])
        }
        4 => {
            let vec = b.type_vector(float, 4);
            let z = b.constant_bit32(float, 0.0f32.to_bits());
            b.constant_composite(vec, vec![z, z, z, z])
        }
        _ => b.constant_bit32(float, 0.0f32.to_bits()),
    }
}

fn float_one(b: &mut Builder, ty: &Type) -> u32 {
    let float = b.type_float(32);
    match numeric_dim(ty) {
        2 => {
            let vec = b.type_vector(float, 2);
            let o = b.constant_bit32(float, 1.0f32.to_bits());
            b.constant_composite(vec, vec![o, o])
        }
        3 => {
            let vec = b.type_vector(float, 3);
            let o = b.constant_bit32(float, 1.0f32.to_bits());
            b.constant_composite(vec, vec![o, o, o])
        }
        4 => {
            let vec = b.type_vector(float, 4);
            let o = b.constant_bit32(float, 1.0f32.to_bits());
            b.constant_composite(vec, vec![o, o, o, o])
        }
        _ => b.constant_bit32(float, 1.0f32.to_bits()),
    }
}

fn int_zero(b: &mut Builder, ty: &Type) -> u32 {
    let int = b.type_int(32, 1);
    match numeric_dim(ty) {
        2 => {
            let vec = b.type_vector(int, 2);
            let z = b.constant_bit32(int, 0);
            b.constant_composite(vec, vec![z, z])
        }
        3 => {
            let vec = b.type_vector(int, 3);
            let z = b.constant_bit32(int, 0);
            b.constant_composite(vec, vec![z, z, z])
        }
        4 => {
            let vec = b.type_vector(int, 4);
            let z = b.constant_bit32(int, 0);
            b.constant_composite(vec, vec![z, z, z, z])
        }
        _ => b.constant_bit32(int, 0),
    }
}

fn int_one(b: &mut Builder, ty: &Type) -> u32 {
    let int = b.type_int(32, 1);
    match numeric_dim(ty) {
        2 => {
            let vec = b.type_vector(int, 2);
            let o = b.constant_bit32(int, 1);
            b.constant_composite(vec, vec![o, o])
        }
        3 => {
            let vec = b.type_vector(int, 3);
            let o = b.constant_bit32(int, 1);
            b.constant_composite(vec, vec![o, o, o])
        }
        4 => {
            let vec = b.type_vector(int, 4);
            let o = b.constant_bit32(int, 1);
            b.constant_composite(vec, vec![o, o, o, o])
        }
        _ => b.constant_bit32(int, 1),
    }
}

fn uint_zero(b: &mut Builder, ty: &Type) -> u32 {
    let uint = b.type_int(32, 0);
    match numeric_dim(ty) {
        2 => {
            let vec = b.type_vector(uint, 2);
            let z = b.constant_bit32(uint, 0);
            b.constant_composite(vec, vec![z, z])
        }
        3 => {
            let vec = b.type_vector(uint, 3);
            let z = b.constant_bit32(uint, 0);
            b.constant_composite(vec, vec![z, z, z])
        }
        4 => {
            let vec = b.type_vector(uint, 4);
            let z = b.constant_bit32(uint, 0);
            b.constant_composite(vec, vec![z, z, z, z])
        }
        _ => b.constant_bit32(uint, 0),
    }
}

fn uint_one(b: &mut Builder, ty: &Type) -> u32 {
    let uint = b.type_int(32, 0);
    match numeric_dim(ty) {
        2 => {
            let vec = b.type_vector(uint, 2);
            let o = b.constant_bit32(uint, 1);
            b.constant_composite(vec, vec![o, o])
        }
        3 => {
            let vec = b.type_vector(uint, 3);
            let o = b.constant_bit32(uint, 1);
            b.constant_composite(vec, vec![o, o, o])
        }
        4 => {
            let vec = b.type_vector(uint, 4);
            let o = b.constant_bit32(uint, 1);
            b.constant_composite(vec, vec![o, o, o, o])
        }
        _ => b.constant_bit32(uint, 1),
    }
}

fn is_storage_buffer(ty: &Type) -> bool {
    matches!(ty, Type::Named { name, .. } if name == "StorageBuffer")
}

fn is_local_size_param(name: &str) -> bool {
    matches!(name, "LOCAL_SIZE_X" | "LOCAL_SIZE_Y" | "LOCAL_SIZE_Z")
}

fn is_permutation_param(name: &str) -> bool {
    if name.starts_with("CFG_")
        || name.starts_with("ENABLE_")
        || name.starts_with("USE_")
        || name.starts_with("WITH_")
        || name.starts_with("HAS_")
        || name.starts_with("ALLOW_")
        || name.starts_with("SUPPORT_")
    {
        return true;
    }
    name.len() >= 4
        && name.contains('_')
        && name
            .chars()
            .all(|c| c.is_uppercase() || c == '_' || c.is_ascii_digit())
}

fn emit_permutation_spec_constant(b: &mut Builder, ty: &Type, spec_id: u32) -> Option<u32> {
    let id = match ty {
        Type::Named { name, .. } if name == "Bool" => {
            let bool_ty = b.type_bool();
            b.spec_constant_false(bool_ty)
        }
        Type::Named { name, .. } if name == "UInt" || name == "u32" => {
            let uint_ty = b.type_int(32, 0);
            b.spec_constant_bit32(uint_ty, 0)
        }
        Type::Named { name, .. } if name == "Int" || name == "i32" => {
            let int_ty = b.type_int(32, 1);
            b.spec_constant_bit32(int_ty, 0)
        }
        Type::Named { name, .. } if name == "Float" || name == "f32" => {
            let float_ty = b.type_float(32);
            b.spec_constant_bit32(float_ty, 0f32.to_bits())
        }
        _ => return None,
    };
    b.decorate(id, Decoration::SpecId, vec![Operand::LiteralBit32(spec_id)]);
    Some(id)
}

fn emit_u32_spec_constant(b: &mut Builder, value: u32, spec_id: u32) -> u32 {
    let uint_ty = b.type_int(32, 0);
    let id = b.spec_constant_bit32(uint_ty, value);
    b.decorate(id, Decoration::SpecId, vec![Operand::LiteralBit32(spec_id)]);
    id
}

fn storage_buffer_elem_type(buffer_ty: &Type, span: kain_core::span::Span) -> Type {
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

fn storage_buffer_stride(buffer_ty: &Type) -> u32 {
    let elem_ty = storage_buffer_elem_type(buffer_ty, kain_core::span::Span::default());
    match elem_ty {
        Type::Named { ref name, .. } if name == "Vec4" || name == "UVec4" => 16,
        Type::Named { ref name, .. } if name == "Vec3" || name == "UVec3" => 12,
        Type::Named { ref name, .. } if name == "Vec2" || name == "UVec2" => 8,
        _ => 4,
    }
}

fn coerce_to_u32_index(
    ctx: &mut ShaderContext,
    value_id: u32,
    value_ty: &Type,
    span: kain_core::span::Span,
) -> KainResult<u32> {
    let uint_ty = ctx.b.type_int(32, 0);
    if is_uint(value_ty) {
        return Ok(value_id);
    }
    if is_int(value_ty) {
        // Reinterpret signed 32-bit index as unsigned.
        return Ok(ctx.b.bitcast(uint_ty, None, value_id).unwrap());
    }
    if is_float(value_ty) {
        return Ok(ctx.b.convert_f_to_u(uint_ty, None, value_id).unwrap());
    }
    if is_bool(value_ty) {
        let one = ctx.b.constant_bit32(uint_ty, 1);
        let zero = ctx.b.constant_bit32(uint_ty, 0);
        return Ok(ctx.b.select(uint_ty, None, value_id, one, zero).unwrap());
    }
    Err(KainError::codegen(
        "Shader index expression must be scalar Int/UInt/Float/Bool",
        span,
    ))
}

fn cast_to_u32(ctx: &mut ShaderContext, value_id: u32, value_ty: &Type) -> u32 {
    if is_uint_like(value_ty) {
        return value_id;
    }

    let dim = numeric_dim(value_ty);
    let scalar_u32 = ctx.b.type_int(32, 0);
    let target_ty = if dim > 1 {
        ctx.b.type_vector(scalar_u32, dim as u32)
    } else {
        scalar_u32
    };

    if is_int_like(value_ty) {
        return ctx.b.bitcast(target_ty, None, value_id).unwrap();
    }
    if is_float_like(value_ty) {
        return ctx.b.convert_f_to_u(target_ty, None, value_id).unwrap();
    }
    if is_bool(value_ty) {
        let one = ctx.b.constant_bit32(scalar_u32, 1);
        let zero = ctx.b.constant_bit32(scalar_u32, 0);
        return ctx.b.select(target_ty, None, value_id, one, zero).unwrap();
    }

    value_id
}

fn cast_to_i32(ctx: &mut ShaderContext, value_id: u32, value_ty: &Type) -> u32 {
    if is_int_like(value_ty) {
        return value_id;
    }

    let dim = numeric_dim(value_ty);
    let scalar_i32 = ctx.b.type_int(32, 1);
    let target_ty = if dim > 1 {
        ctx.b.type_vector(scalar_i32, dim as u32)
    } else {
        scalar_i32
    };

    if is_uint_like(value_ty) {
        return ctx.b.bitcast(target_ty, None, value_id).unwrap();
    }
    if is_float_like(value_ty) {
        return ctx.b.convert_f_to_s(target_ty, None, value_id).unwrap();
    }
    if is_bool(value_ty) {
        let one = ctx.b.constant_bit32(scalar_i32, 1);
        let zero = ctx.b.constant_bit32(scalar_i32, 0);
        return ctx.b.select(target_ty, None, value_id, one, zero).unwrap();
    }

    value_id
}

fn cast_to_f32(ctx: &mut ShaderContext, value_id: u32, value_ty: &Type) -> u32 {
    if is_float_like(value_ty) {
        return value_id;
    }

    let dim = numeric_dim(value_ty);
    let scalar_f32 = ctx.b.type_float(32);
    let target_ty = if dim > 1 {
        ctx.b.type_vector(scalar_f32, dim as u32)
    } else {
        scalar_f32
    };

    if is_int_like(value_ty) {
        return ctx.b.convert_s_to_f(target_ty, None, value_id).unwrap();
    }
    if is_uint_like(value_ty) {
        return ctx.b.convert_u_to_f(target_ty, None, value_id).unwrap();
    }
    if is_bool(value_ty) {
        let one = float_one(ctx.b, value_ty);
        let zero = float_zero(ctx.b, value_ty);
        return ctx.b.select(target_ty, None, value_id, one, zero).unwrap();
    }

    value_id
}

fn splat_to_vec(ctx: &mut ShaderContext, scalar_id: u32, scalar_ty: &Type, width: usize) -> u32 {
    if width <= 1 {
        return scalar_id;
    }
    let scalar_spv = map_ast_type(ctx.b, scalar_ty);
    let vec_spv = ctx.b.type_vector(scalar_spv, width as u32);
    let mut comps = Vec::with_capacity(width);
    for _ in 0..width {
        comps.push(scalar_id);
    }
    ctx.b.composite_construct(vec_spv, None, comps).unwrap()
}

fn coerce_float_binary_operands(
    ctx: &mut ShaderContext,
    lhs_id: u32,
    lhs_ty: &Type,
    rhs_id: u32,
    rhs_ty: &Type,
) -> (u32, u32, Type) {
    let lhs_f = cast_to_f32(ctx, lhs_id, lhs_ty);
    let rhs_f = cast_to_f32(ctx, rhs_id, rhs_ty);
    let lhs_dim = numeric_dim(lhs_ty);
    let rhs_dim = numeric_dim(rhs_ty);

    if lhs_dim > 1 && rhs_dim == 1 {
        let rhs_scalar_ty = Type::Named {
            name: "Float".into(),
            generics: vec![],
            span: rhs_ty.span(),
        };
        let rhs_splat = splat_to_vec(ctx, rhs_f, &rhs_scalar_ty, lhs_dim);
        let out_ty = vec_type_from_scalar(
            &Type::Named {
                name: "Float".into(),
                generics: vec![],
                span: lhs_ty.span(),
            },
            lhs_dim,
            lhs_ty.span(),
        );
        return (lhs_f, rhs_splat, out_ty);
    }
    if rhs_dim > 1 && lhs_dim == 1 {
        let lhs_scalar_ty = Type::Named {
            name: "Float".into(),
            generics: vec![],
            span: lhs_ty.span(),
        };
        let lhs_splat = splat_to_vec(ctx, lhs_f, &lhs_scalar_ty, rhs_dim);
        let out_ty = vec_type_from_scalar(
            &Type::Named {
                name: "Float".into(),
                generics: vec![],
                span: rhs_ty.span(),
            },
            rhs_dim,
            rhs_ty.span(),
        );
        return (lhs_splat, rhs_f, out_ty);
    }
    if lhs_dim > 1 {
        let out_ty = vec_type_from_scalar(
            &Type::Named {
                name: "Float".into(),
                generics: vec![],
                span: lhs_ty.span(),
            },
            lhs_dim,
            lhs_ty.span(),
        );
        return (lhs_f, rhs_f, out_ty);
    }

    (
        lhs_f,
        rhs_f,
        Type::Named {
            name: "Float".into(),
            generics: vec![],
            span: lhs_ty.span(),
        },
    )
}

fn coerce_float_ternary_operands(
    ctx: &mut ShaderContext,
    a_id: u32,
    a_ty: &Type,
    b_id: u32,
    b_ty: &Type,
    c_id: u32,
    c_ty: &Type,
) -> (u32, u32, u32, Type) {
    let a_f = cast_to_f32(ctx, a_id, a_ty);
    let b_f = cast_to_f32(ctx, b_id, b_ty);
    let c_f = cast_to_f32(ctx, c_id, c_ty);
    let out_dim = numeric_dim(a_ty)
        .max(numeric_dim(b_ty))
        .max(numeric_dim(c_ty));

    if out_dim <= 1 {
        return (
            a_f,
            b_f,
            c_f,
            Type::Named {
                name: "Float".into(),
                generics: vec![],
                span: a_ty.span(),
            },
        );
    }

    let scalar_ty = Type::Named {
        name: "Float".into(),
        generics: vec![],
        span: a_ty.span(),
    };
    let a_out = if numeric_dim(a_ty) == 1 {
        splat_to_vec(ctx, a_f, &scalar_ty, out_dim)
    } else {
        a_f
    };
    let b_out = if numeric_dim(b_ty) == 1 {
        splat_to_vec(ctx, b_f, &scalar_ty, out_dim)
    } else {
        b_f
    };
    let c_out = if numeric_dim(c_ty) == 1 {
        splat_to_vec(ctx, c_f, &scalar_ty, out_dim)
    } else {
        c_f
    };
    let out_ty = vec_type_from_scalar(&scalar_ty, out_dim, a_ty.span());
    (a_out, b_out, c_out, out_ty)
}

fn vector_scalar_type(ctx: &mut ShaderContext, vec_ty: &Type) -> Type {
    if matches!(vec_ty, Type::Named { name, .. } if name.starts_with('U')) {
        Type::Named {
            name: "UInt".into(),
            generics: vec![],
            span: kain_core::span::Span::default(),
        }
    } else if matches!(vec_ty, Type::Named { name, .. } if name.starts_with('I')) {
        Type::Named {
            name: "Int".into(),
            generics: vec![],
            span: kain_core::span::Span::default(),
        }
    } else if is_uint(vec_ty) {
        Type::Named {
            name: "UInt".into(),
            generics: vec![],
            span: kain_core::span::Span::default(),
        }
    } else if is_int(vec_ty) {
        Type::Named {
            name: "Int".into(),
            generics: vec![],
            span: kain_core::span::Span::default(),
        }
    } else {
        let _ = ctx; // keep signature uniform for future extension
        Type::Named {
            name: "Float".into(),
            generics: vec![],
            span: kain_core::span::Span::default(),
        }
    }
}

fn vec_type_from_scalar(scalar_ty: &Type, width: usize, span: kain_core::span::Span) -> Type {
    let is_u = matches!(scalar_ty, Type::Named { name, .. } if name == "UInt" || name == "u32");
    let is_i = matches!(scalar_ty, Type::Named { name, .. } if name == "Int" || name == "i32");
    let name = if is_u {
        match width {
            2 => "UVec2",
            3 => "UVec3",
            _ => "UVec4",
        }
    } else if is_i {
        match width {
            2 => "IVec2",
            3 => "IVec3",
            _ => "IVec4",
        }
    } else {
        match width {
            2 => "Vec2",
            3 => "Vec3",
            _ => "Vec4",
        }
    };
    Type::Named {
        name: name.into(),
        generics: vec![],
        span,
    }
}

fn swizzle_indices(field: &str) -> Option<Vec<u32>> {
    if field.is_empty() || field.len() > 4 {
        return None;
    }
    let mut out = Vec::with_capacity(field.len());
    for ch in field.chars() {
        let idx = match ch {
            'x' | 'r' => 0,
            'y' | 'g' => 1,
            'z' | 'b' => 2,
            'w' | 'a' => 3,
            _ => return None,
        };
        out.push(idx);
    }
    Some(out)
}

fn emit_index_lvalue_ptr(
    ctx: &mut ShaderContext,
    object: &Expr,
    index: &Expr,
    elem_ty: &Type,
    span: kain_core::span::Span,
) -> KainResult<u32> {
    if let Expr::Ident(buffer_name, _) = object {
        if ctx.storage_buffers.contains(buffer_name) {
            let binding = ctx.vars.get(buffer_name).cloned().ok_or_else(|| {
                KainError::codegen(format!("Unknown storage buffer: {}", buffer_name), span)
            })?;
            let (index_raw_id, index_raw_ty) = emit_expr(ctx, index)?;
            let index_id = coerce_to_u32_index(ctx, index_raw_id, &index_raw_ty, span)?;
            let elem_ty_id = map_ast_type(ctx.b, elem_ty);
            let elem_ptr_ty = ctx
                .b
                .type_pointer(None, StorageClass::StorageBuffer, elem_ty_id);
            let uint_ty = ctx.b.type_int(32, 0);
            let zero = ctx.b.constant_bit32(uint_ty, 0);
            let elem_ptr = ctx
                .b
                .access_chain(elem_ptr_ty, None, binding.id, vec![zero, index_id])
                .unwrap();
            return Ok(elem_ptr);
        }
    }
    Err(KainError::codegen(
        "Unsupported lvalue index target in shader",
        span,
    ))
}
